from __future__ import annotations

import json
import time
from dataclasses import dataclass
from typing import Any, Literal, Sequence

from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.signal_sheet import SignalSheetRepository
from app.core.config import get_settings
from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.schemas.ws.events import (
    ExternalIedPlanningChangedEvent,
    ExternalIedPlanningEndpointRecord,
    ExternalIedPlanningSignalResult,
    ExternalIedPlanningSnapshotEvent,
)
from app.services.discovery_planner import (
    DiscoveryCache,
    DiscoveryCacheDataset,
    DiscoveryCacheFcda,
    DiscoveryCacheIed,
    DiscoveryCacheRcb,
    DiscoveryPlanSignalInput,
    DiscoveryPlanner,
    VerificationPlan,
    VerificationPlanSignal,
)
from app.services.external_ied_discovery_scheduler import (
    _cache_key,
    _discovery_model_key,
    discovery_cache_metadata_from_payload,
)

logger = get_logger("external_ied_planning")

PlanningRequestReason = Literal["discovery_completed", "mapping_changed", "verification_required", "user_refresh"]
PlanningState = Literal["NotPlanned", "Queued", "Running", "Ready", "Partial", "Failed", "Stale", "WaitingForDiscovery"]


@dataclass(frozen=True, slots=True)
class ExternalIedPlanningRequest:
    request_id: str
    workspace_id: int
    endpoint: str
    ip: str
    port: int
    signal_ids: tuple[int, ...]
    reason: PlanningRequestReason
    model_fingerprint: str | None = None
    planning_fingerprint: str | None = None
    requested_at_ms: int = 0

    def to_payload(self) -> dict[str, Any]:
        return {
            "event": "ExternalIedPlanningRequested",
            "request_id": self.request_id,
            "workspace_id": self.workspace_id,
            "endpoint": self.endpoint,
            "ip": self.ip,
            "port": self.port,
            "signal_ids": list(self.signal_ids),
            "reason": self.reason,
            "model_fingerprint": self.model_fingerprint,
            "planning_fingerprint": self.planning_fingerprint,
            "requested_at_ms": self.requested_at_ms,
        }

    @classmethod
    def from_payload(cls, payload: dict[str, Any]) -> ExternalIedPlanningRequest:
        endpoint = str(payload.get("endpoint") or "")
        ip = str(payload.get("ip") or "").strip()
        raw_port = payload.get("port")
        if not ip and endpoint:
            ip = endpoint.rsplit(":", 1)[0]
        port = _normalize_port(raw_port)
        if endpoint and ":" in endpoint:
            try:
                port = _normalize_port(endpoint.rsplit(":", 1)[1])
            except ValueError:
                port = _normalize_port(raw_port)
        if not endpoint and ip:
            endpoint = f"{ip}:{port}"
        event = str(payload.get("event") or "")
        reason = _planning_reason(payload.get("reason"), event=event)
        return cls(
            request_id=str(payload.get("request_id") or ""),
            workspace_id=int(payload.get("workspace_id") or 0),
            endpoint=endpoint,
            ip=ip,
            port=port,
            signal_ids=_normalize_signal_ids(payload.get("signal_ids")),
            reason=reason,
            model_fingerprint=_str_or_none(payload.get("model_fingerprint")),
            planning_fingerprint=_str_or_none(payload.get("planning_fingerprint")),
            requested_at_ms=_int_or_none(payload.get("requested_at_ms")) or _int_or_none(payload.get("completed_at_ms")) or int(time.time() * 1000),
        )


async def emit_external_ied_planning_requested(
    *,
    workspace_id: int,
    endpoint: str,
    ip: str,
    port: int,
    signal_ids: Sequence[int],
    reason: PlanningRequestReason = "mapping_changed",
) -> str:
    settings = get_settings()
    redis = RedisManager.get_instance()
    request = ExternalIedPlanningRequest(
        request_id=f"plan-{workspace_id}-{endpoint}-{int(time.time() * 1000)}",
        workspace_id=workspace_id,
        endpoint=endpoint,
        ip=ip,
        port=port,
        signal_ids=tuple(sorted({int(signal_id) for signal_id in signal_ids if int(signal_id) > 0})),
        reason=reason,
        requested_at_ms=int(time.time() * 1000),
    )
    return await redis.xadd(
        settings.external_ied_planning_event_stream,
        {"data": json.dumps(request.to_payload(), separators=(",", ":"))},
        maxlen=settings.external_ied_planning_event_stream_maxlen,
        approximate=True,
    )


async def execute_external_ied_planning_request(
    request: ExternalIedPlanningRequest,
    *,
    db: AsyncSession,
) -> VerificationPlan | None:
    redis = RedisManager.get_instance()
    await _store_planning_endpoint(
        redis,
        workspace_id=request.workspace_id,
        endpoint=request.endpoint,
        payload=_endpoint_payload_from_request(request, state="Running"),
    )
    try:
        rows = await _load_mapped_rows(db, request)
        cache = await _load_discovery_cache(redis, workspace_id=request.workspace_id, endpoint=request.endpoint)
        if cache is None:
            endpoint_payload = _endpoint_payload_from_request(request, state="WaitingForDiscovery", signal_ids=[row.signal_id for row in rows])
            await _store_planning_endpoint(redis, workspace_id=request.workspace_id, endpoint=request.endpoint, payload=endpoint_payload)
            await _publish_planning_changed(request.workspace_id, endpoint_payload, [], [])
            return None

        signals = [_signal_input_from_row(row) for row in rows]
        plan = DiscoveryPlanner().build_plan(signal_list=signals, discovery_cache=cache)
        signal_results = [_signal_result_from_plan_signal(request.endpoint, signal) for signal in plan.signals]
        endpoint_payload = _endpoint_payload_from_plan(request, plan, signal_results)
        previous_signal_ids = await _signal_ids_for_endpoint(redis, workspace_id=request.workspace_id, endpoint=request.endpoint)
        next_signal_ids = {result["signal_id"] for result in signal_results}
        removed_signal_ids = sorted(previous_signal_ids.difference(next_signal_ids))
        await _store_planning_endpoint(redis, workspace_id=request.workspace_id, endpoint=request.endpoint, payload=endpoint_payload)
        await _store_signal_results(redis, workspace_id=request.workspace_id, endpoint=request.endpoint, signal_results=signal_results)
        if removed_signal_ids:
            await _remove_signal_results(redis, workspace_id=request.workspace_id, signal_ids=removed_signal_ids)
        await _store_verification_plan(redis, workspace_id=request.workspace_id, endpoint=request.endpoint, plan=plan)
        await _publish_planning_changed(request.workspace_id, endpoint_payload, signal_results, removed_signal_ids)
        return plan
    except Exception as exc:  # noqa: BLE001
        endpoint_payload = _endpoint_payload_from_request(request, state="Failed", last_error=str(exc))
        await _store_planning_endpoint(redis, workspace_id=request.workspace_id, endpoint=request.endpoint, payload=endpoint_payload)
        await _publish_planning_changed(request.workspace_id, endpoint_payload, [], [])
        raise


async def build_external_ied_planning_snapshot(workspace_id: int) -> ExternalIedPlanningSnapshotEvent:
    redis = RedisManager.get_instance()
    endpoint_payloads = await redis.hgetall(_planning_state_key(workspace_id))
    signal_payloads = await redis.hgetall(_planning_signal_key(workspace_id))
    endpoints = [
        ExternalIedPlanningEndpointRecord.model_validate(_parse_payload(payload))
        for payload in endpoint_payloads.values()
        if _parse_payload(payload) is not None
    ]
    signal_results = [
        ExternalIedPlanningSignalResult.model_validate(_parse_payload(payload))
        for payload in signal_payloads.values()
        if _parse_payload(payload) is not None
    ]
    return ExternalIedPlanningSnapshotEvent(
        workspace_id=workspace_id,
        endpoints=sorted(endpoints, key=lambda item: item.endpoint),
        signal_results=sorted(signal_results, key=lambda item: item.signal_id),
        removed_signal_ids=[],
        emitted_at=_utc_now_iso(),
    )


async def list_external_ied_planning_snapshots() -> list[ExternalIedPlanningSnapshotEvent]:
    redis = RedisManager.get_instance()
    raw_workspace_ids = await redis.smembers("external_ied:workspaces")
    events: list[ExternalIedPlanningSnapshotEvent] = []
    for raw_workspace_id in raw_workspace_ids:
        try:
            workspace_id = int(raw_workspace_id)
        except (TypeError, ValueError):
            continue
        events.append(await build_external_ied_planning_snapshot(workspace_id))
    return events


async def load_external_ied_planning_endpoint_fields(workspace_id: int, endpoint: str) -> dict[str, Any]:
    redis = RedisManager.get_instance()
    payload = _parse_payload(await redis.hget(_planning_state_key(workspace_id), endpoint)) or {}
    return {
        "planning_state": payload.get("state") or "NotPlanned",
        "planning_updated_at_ms": payload.get("updated_at_ms"),
        "planning_matched_count": payload.get("matched_count") or 0,
        "planning_unmatched_count": payload.get("unmatched_count") or 0,
        "planning_ambiguous_count": payload.get("ambiguous_count") or 0,
        "planning_last_error": payload.get("last_error"),
    }


async def _load_mapped_rows(db: AsyncSession, request: ExternalIedPlanningRequest) -> list[SignalAllocationRowSchema]:
    repo = SignalSheetRepository(db)
    rows = (
        await repo.list_allocation_rows_by_signal_ids(request.workspace_id, request.signal_ids)
        if request.signal_ids
        else await repo.list_allocation_rows(request.workspace_id)
    )
    result: list[SignalAllocationRowSchema] = []
    for row in rows:
        if not _verification_enabled(row.signal_metadata):
            continue
        if _endpoint_from_row(row) != request.endpoint:
            continue
        if not _iec61850_address_from_row(row):
            continue
        result.append(row)
    return result


async def _load_discovery_cache(redis, *, workspace_id: int, endpoint: str) -> DiscoveryCache | None:
    cache_payload = await redis.hget(_cache_key(workspace_id), endpoint)
    model_payload = await redis.hget(_discovery_model_key(workspace_id), endpoint)
    metadata = discovery_cache_metadata_from_payload(cache_payload)
    model = _parse_payload(model_payload)
    if metadata is None or model is None or not metadata.model_fingerprint:
        return None
    fcdas = tuple(
        DiscoveryCacheFcda(reference=str(item.get("reference") or ""), fc=_str_or_none(item.get("fc")))
        for item in model.get("fcdas", [])
        if isinstance(item, dict) and str(item.get("reference") or "").strip()
    )
    datasets = tuple(
        DiscoveryCacheDataset(
            reference=str(item.get("reference") or ""),
            members=tuple(str(member) for member in item.get("members", []) if str(member).strip()),
        )
        for item in model.get("datasets", [])
        if isinstance(item, dict) and str(item.get("reference") or "").strip()
    )
    rcbs = tuple(
        DiscoveryCacheRcb(
            reference=str(item.get("reference") or ""),
            name=str(item.get("name") or item.get("reference") or ""),
            dataset_reference=str(item.get("dataset_reference") or ""),
            kind=_rcb_kind(item.get("kind")),
            conf_rev=_str_or_none(item.get("conf_rev")),
        )
        for item in model.get("rcbs", [])
        if isinstance(item, dict) and str(item.get("reference") or "").strip() and str(item.get("dataset_reference") or "").strip()
    )
    return DiscoveryCache(ieds=(DiscoveryCacheIed(metadata=metadata, fcdas=fcdas, datasets=datasets, rcbs=rcbs),))


def _signal_input_from_row(row: SignalAllocationRowSchema) -> DiscoveryPlanSignalInput:
    return DiscoveryPlanSignalInput(
        signal_id=row.signal_id,
        address=_iec61850_address_from_row(row) or "",
        source_row_id=row.row_id,
    )


def _signal_result_from_plan_signal(endpoint: str, signal: VerificationPlanSignal) -> dict[str, Any]:
    status = "matched" if signal.status == "planned" else _planning_signal_status_from_reason(signal.reason)
    return {
        "signal_id": signal.signal_id,
        "endpoint": endpoint,
        "status": status,
        "address": signal.address,
        "reason": signal.reason,
        "ied_identity": signal.ied_identity,
        "fcda_reference": signal.fcda_reference,
        "dataset_reference": signal.dataset_reference,
        "rcb_reference": signal.rcb_reference,
        "rcb_name": signal.rcb_name,
    }


def _endpoint_payload_from_request(
    request: ExternalIedPlanningRequest,
    *,
    state: PlanningState,
    signal_ids: Sequence[int] | None = None,
    last_error: str | None = None,
) -> dict[str, Any]:
    return {
        "endpoint": request.endpoint,
        "ip": request.ip,
        "port": request.port,
        "state": state,
        "planning_fingerprint": request.planning_fingerprint,
        "model_fingerprint": request.model_fingerprint,
        "updated_at_ms": int(time.time() * 1000),
        "matched_count": 0,
        "unmatched_count": 0,
        "ambiguous_count": 0,
        "signal_ids": sorted({int(signal_id) for signal_id in (signal_ids or request.signal_ids) if int(signal_id) > 0}),
        "last_error": last_error,
    }


def _endpoint_payload_from_plan(
    request: ExternalIedPlanningRequest,
    plan: VerificationPlan,
    signal_results: Sequence[dict[str, Any]],
) -> dict[str, Any]:
    matched = sum(1 for result in signal_results if result["status"] == "matched")
    unmatched = sum(1 for result in signal_results if result["status"] == "unmatched")
    ambiguous = sum(1 for result in signal_results if result["status"] == "ambiguous")
    stale = sum(1 for result in signal_results if result["status"] == "stale")
    state: PlanningState
    if not signal_results:
        state = "NotPlanned"
    elif stale and unmatched == 0 and ambiguous == 0:
        state = "Stale"
    elif unmatched or ambiguous or stale:
        state = "Partial"
    else:
        state = "Ready"
    model_fingerprint = next(iter(plan.model_fingerprints.values()), request.model_fingerprint)
    return {
        "endpoint": request.endpoint,
        "ip": request.ip,
        "port": request.port,
        "state": state,
        "planning_fingerprint": plan.planning_fingerprint,
        "model_fingerprint": model_fingerprint,
        "updated_at_ms": int(time.time() * 1000),
        "matched_count": matched,
        "unmatched_count": unmatched,
        "ambiguous_count": ambiguous,
        "signal_ids": sorted(result["signal_id"] for result in signal_results),
        "last_error": None,
    }


def _planning_signal_status_from_reason(reason: str | None) -> Literal["unmatched", "stale"]:
    if reason == "discovery cache has no dataset members for IED domain":
        return "stale"
    return "unmatched"


async def _publish_planning_changed(
    workspace_id: int,
    endpoint_payload: dict[str, Any],
    signal_results: Sequence[dict[str, Any]],
    removed_signal_ids: Sequence[int],
) -> None:
    event = ExternalIedPlanningChangedEvent(
        workspace_id=workspace_id,
        endpoint=ExternalIedPlanningEndpointRecord.model_validate(endpoint_payload),
        signal_results=[ExternalIedPlanningSignalResult.model_validate(result) for result in signal_results],
        removed_signal_ids=list(removed_signal_ids),
        emitted_at=_utc_now_iso(),
    )
    await WsEventPublisher.publish(event)


async def _store_planning_endpoint(redis, *, workspace_id: int, endpoint: str, payload: dict[str, Any]) -> None:
    await redis.hset(_planning_state_key(workspace_id), endpoint, json.dumps(payload, separators=(",", ":"), sort_keys=True))


async def _store_signal_results(
    redis,
    *,
    workspace_id: int,
    endpoint: str,
    signal_results: Sequence[dict[str, Any]],
) -> None:
    for result in signal_results:
        await redis.hset(
            _planning_signal_key(workspace_id),
            str(result["signal_id"]),
            json.dumps({**result, "endpoint": endpoint}, separators=(",", ":"), sort_keys=True),
        )


async def _remove_signal_results(redis, *, workspace_id: int, signal_ids: Sequence[int]) -> None:
    for signal_id in signal_ids:
        await redis.hdel(_planning_signal_key(workspace_id), str(signal_id))


async def _store_verification_plan(redis, *, workspace_id: int, endpoint: str, plan: VerificationPlan) -> None:
    await redis.hset(_planning_plan_key(workspace_id), endpoint, json.dumps(_plan_payload(plan), separators=(",", ":"), sort_keys=True))


async def _signal_ids_for_endpoint(redis, *, workspace_id: int, endpoint: str) -> set[int]:
    payloads = await redis.hgetall(_planning_signal_key(workspace_id))
    result: set[int] = set()
    for payload in payloads.values():
        parsed = _parse_payload(payload)
        if parsed is None or parsed.get("endpoint") != endpoint:
            continue
        signal_id = _int_or_none(parsed.get("signal_id"))
        if signal_id is not None:
            result.add(signal_id)
    return result


def _plan_payload(plan: VerificationPlan) -> dict[str, Any]:
    return {
        "plan_id": plan.plan_id,
        "planning_fingerprint": plan.planning_fingerprint,
        "model_fingerprints": plan.model_fingerprints,
        "diagnostics": list(plan.diagnostics),
        "action": plan.action,
        "discovery_required": plan.discovery_required,
        "signals": [
            {
                "signal_id": signal.signal_id,
                "address": signal.address,
                "status": signal.status,
                "ied_identity": signal.ied_identity,
                "fcda_reference": signal.fcda_reference,
                "functional_constraint": signal.functional_constraint,
                "dataset_reference": signal.dataset_reference,
                "rcb_reference": signal.rcb_reference,
                "rcb_name": signal.rcb_name,
                "reason": signal.reason,
            }
            for signal in plan.signals
        ],
        "dependency_graph": {
            "nodes": list(plan.dependency_graph.nodes),
            "edges": list(plan.dependency_graph.edges),
        },
    }


def _planning_state_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:planning_state"


def _planning_signal_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:planning_signal"


def _planning_plan_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:verification_plan"


def _verification_enabled(metadata: dict[str, Any]) -> bool:
    verification = metadata.get("verification") if isinstance(metadata, dict) else None
    return isinstance(verification, dict) and verification.get("enabled") is True


def _endpoint_from_row(row: SignalAllocationRowSchema) -> str | None:
    host = _transport_host_from_row(row)
    if not host:
        return None
    ip, port = _split_host_port(host)
    return f"{ip}:{port}" if ip else None


def _transport_host_from_row(row: SignalAllocationRowSchema) -> str | None:
    metadata = row.signal_metadata or {}
    verification = metadata.get("verification") if isinstance(metadata.get("verification"), dict) else {}
    source = metadata.get("row") if isinstance(metadata.get("row"), dict) else {}
    for source_map in (verification, source, metadata):
        for key in ("transport_host", "transport_reference", "host", "ip_address", "device_ip", "target_ip", "mms_host", "endpoint_host", "ip"):
            value = _str_or_none(source_map.get(key)) if isinstance(source_map, dict) else None
            if value:
                return value
    return None


def _iec61850_address_from_row(row: SignalAllocationRowSchema) -> str | None:
    metadata = row.signal_metadata or {}
    verification = metadata.get("verification") if isinstance(metadata.get("verification"), dict) else {}
    source = metadata.get("row") if isinstance(metadata.get("row"), dict) else {}
    for source_map in (verification, source, metadata):
        for key in ("iec61850_address", "iec61850", "iec61850_path", "mms_reference", "object_reference", "signal_path", "reference"):
            value = _str_or_none(source_map.get(key)) if isinstance(source_map, dict) else None
            if value:
                return value
    return None


def _split_host_port(value: str) -> tuple[str | None, int]:
    text = value.strip()
    if ":" in text:
        host, raw_port = text.rsplit(":", 1)
        return host.strip() or None, _normalize_port(raw_port)
    return text or None, 102


def _normalize_port(value: Any) -> int:
    port = int(value or 102)
    if port < 1 or port > 65535:
        return 102
    return port


def _normalize_signal_ids(value: Any) -> tuple[int, ...]:
    if not isinstance(value, list | tuple | set):
        return ()
    result = sorted({int(item) for item in value if _int_or_none(item) and int(item) > 0})
    return tuple(result)


def _planning_reason(value: Any, *, event: str) -> PlanningRequestReason:
    if value in {"mapping_changed", "verification_required", "user_refresh", "discovery_completed"}:
        return value
    if event == "ExternalIedDiscoveryCompleted":
        return "discovery_completed"
    return "mapping_changed"


def _rcb_kind(value: Any) -> Literal["buffered", "unbuffered", "unknown"]:
    text = str(value or "").strip().lower()
    if text in {"buffered", "unbuffered"}:
        return text
    return "unknown"


def _parse_payload(payload: str | bytes | dict[str, Any] | None) -> dict[str, Any] | None:
    if isinstance(payload, dict):
        return payload
    if isinstance(payload, bytes):
        payload = payload.decode("utf-8")
    if not isinstance(payload, str) or not payload:
        return None
    try:
        parsed = json.loads(payload)
    except json.JSONDecodeError:
        return None
    return parsed if isinstance(parsed, dict) else None


def _str_or_none(value: Any) -> str | None:
    text = str(value or "").strip()
    return text or None


def _int_or_none(value: Any) -> int | None:
    try:
        number = int(value)
    except (TypeError, ValueError):
        return None
    return number if number >= 0 else None


def _utc_now_iso() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
