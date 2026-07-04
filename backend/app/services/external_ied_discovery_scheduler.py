from __future__ import annotations

import json
import time
from dataclasses import dataclass
from typing import Any, Callable, Literal, Protocol
from uuid import uuid4

from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.events import ExternalIedStatusRecord

logger = get_logger("external_ied_discovery")
_REDIS_SCHEDULER: ExternalIedDiscoveryScheduler | None = None
EXTERNAL_IED_DISCOVERY_VERSION = "unitlab.external-ied.discovery.v2"

DiscoveryScheduleReason = Literal[
    "reachable_stable",
    "reachable_candidate_verification_pending",
    "user_refresh",
    "cache_missing",
    "cache_stale",
    "verification_required",
]
DiscoveryDecisionKind = Literal[
    "NoAction",
    "QueueDiscovery",
    "QueueHighPriorityDiscovery",
    "Ignore",
    "WaitForStableConnection",
]
DiscoveryPriority = Literal["normal", "high"]
DiscoveryLifecycleState = Literal[
    "NeverDiscovered",
    "Queued",
    "Running",
    "Succeeded",
    "Failed",
    "RetryWaiting",
    "Cancelled",
    "Stale",
]

REACHABLE_CANDIDATE_REASON = "reachable_candidate"
REACHABLE_STABLE_REASON = "reachable_stable"
SUSPECT_OFFLINE_REASON = "suspect_offline"

BLOCKED_FAILURE_CODES = {"network_unreachable", "mms_unavailable"}
BLOCKED_STATUSES = {"offline", "not_applicable"}


@dataclass(frozen=True, slots=True)
class ExternalIedDiscoveryEndpointState:
    workspace_id: int
    ip: str
    port: int
    status: str
    signal_ids: tuple[int, ...] = ()
    priority_reason: str | None = None
    failure_code: str | None = None
    last_checked_at: str | None = None

    @property
    def endpoint(self) -> str:
        return f"{self.ip}:{self.port}"


EndpointState = ExternalIedDiscoveryEndpointState


@dataclass(frozen=True, slots=True)
class DiscoveryCacheMetadata:
    discovery_version: str | None = None
    device_identity: str | None = None
    vendor: str | None = None
    model: str | None = None
    config_rev: str | None = None
    last_discovery_at_ms: int | None = None
    last_successful_discovery_at_ms: int | None = None
    last_failed_discovery_at_ms: int | None = None
    last_error: str | None = None
    discovery_duration_ms: int | None = None
    model_fingerprint: str | None = None
    planning_fingerprint: str | None = None

    def to_payload(self) -> dict[str, Any]:
        return {
            "discovery_version": self.discovery_version,
            "device_identity": self.device_identity,
            "vendor": self.vendor,
            "model": self.model,
            "configRev": self.config_rev,
            "last_discovery_at_ms": self.last_discovery_at_ms,
            "last_successful_discovery_at_ms": self.last_successful_discovery_at_ms,
            "last_failed_discovery_at_ms": self.last_failed_discovery_at_ms,
            "last_error": self.last_error,
            "discovery_duration_ms": self.discovery_duration_ms,
            "model_fingerprint": self.model_fingerprint,
            "planning_fingerprint": self.planning_fingerprint,
        }


ExternalIedDiscoveryCacheRecord = DiscoveryCacheMetadata


@dataclass(frozen=True, slots=True)
class VerificationContext:
    verification_pending: bool = False
    verification_required: bool = False
    planning_fingerprint: str | None = None


@dataclass(frozen=True, slots=True)
class UserAction:
    action: Literal["none", "refresh", "discover"] = "none"


@dataclass(slots=True)
class DiscoveryStateMachine:
    state: DiscoveryLifecycleState = "NeverDiscovered"
    retry_at_ms: int | None = None
    queued_at_ms: int | None = None
    running_at_ms: int | None = None
    completed_at_ms: int | None = None
    failed_at_ms: int | None = None
    cancelled_at_ms: int | None = None
    stale_at_ms: int | None = None
    last_error: str | None = None
    updated_at_ms: int | None = None
    queued_request_id: str | None = None
    queued_priority: DiscoveryPriority | None = None
    queued_reason: DiscoveryScheduleReason | None = None
    retry_count: int = 0

    def canSchedule(self, *, now_ms: int) -> bool:
        if self.state in {"Queued", "Running"}:
            return False
        if self.state == "RetryWaiting":
            return self.retry_at_ms is not None and self.retry_at_ms <= now_ms
        return self.state in {"NeverDiscovered", "Succeeded", "Failed", "Cancelled", "Stale"}

    def markQueued(
        self,
        *,
        now_ms: int,
        request_id: str | None = None,
        priority: DiscoveryPriority | None = None,
        reason: DiscoveryScheduleReason | None = None,
    ) -> None:
        if not self.canSchedule(now_ms=now_ms):
            raise ValueError(f"Cannot transition discovery from {self.state} to Queued.")
        self.state = "Queued"
        self.queued_at_ms = now_ms
        self.running_at_ms = None
        self.retry_at_ms = None
        self.queued_request_id = request_id
        self.queued_priority = priority
        self.queued_reason = reason
        self.updated_at_ms = now_ms

    def markRunning(self, *, now_ms: int) -> None:
        if self.state != "Queued":
            raise ValueError(f"Cannot transition discovery from {self.state} to Running.")
        self.state = "Running"
        self.running_at_ms = now_ms
        self.updated_at_ms = now_ms

    def markCompleted(self, *, now_ms: int) -> None:
        if self.state not in {"Queued", "Running"}:
            raise ValueError(f"Cannot transition discovery from {self.state} to Succeeded.")
        self.state = "Succeeded"
        self.completed_at_ms = now_ms
        self.retry_at_ms = None
        self.last_error = None
        self.queued_request_id = None
        self.queued_priority = None
        self.queued_reason = None
        self.retry_count = 0
        self.updated_at_ms = now_ms

    def markFailed(self, *, now_ms: int, error: str | None = None) -> None:
        if self.state not in {"Queued", "Running", "RetryWaiting"}:
            raise ValueError(f"Cannot transition discovery from {self.state} to Failed.")
        self.state = "Failed"
        self.failed_at_ms = now_ms
        self.last_error = error
        self.retry_at_ms = None
        self.queued_request_id = None
        self.queued_priority = None
        self.queued_reason = None
        self.updated_at_ms = now_ms

    def markCancelled(self, *, now_ms: int) -> None:
        if self.state not in {"Queued", "Running", "RetryWaiting"}:
            raise ValueError(f"Cannot transition discovery from {self.state} to Cancelled.")
        self.state = "Cancelled"
        self.cancelled_at_ms = now_ms
        self.retry_at_ms = None
        self.queued_request_id = None
        self.queued_priority = None
        self.queued_reason = None
        self.updated_at_ms = now_ms

    def markRetry(self, *, now_ms: int, retry_at_ms: int, error: str | None = None) -> None:
        if self.state not in {"Queued", "Running", "Failed"}:
            raise ValueError(f"Cannot transition discovery from {self.state} to RetryWaiting.")
        if retry_at_ms <= now_ms:
            raise ValueError("Discovery retry_at_ms must be in the future.")
        self.state = "RetryWaiting"
        self.failed_at_ms = now_ms
        self.retry_at_ms = retry_at_ms
        self.last_error = error
        self.queued_request_id = None
        self.retry_count += 1
        self.updated_at_ms = now_ms

    def markStale(self, *, now_ms: int) -> None:
        if self.state != "Succeeded":
            raise ValueError(f"Cannot transition discovery from {self.state} to Stale.")
        self.state = "Stale"
        self.stale_at_ms = now_ms
        self.updated_at_ms = now_ms

    def isReadyForVerification(self) -> bool:
        return self.state == "Succeeded"

    def to_payload(self) -> dict[str, Any]:
        return {
            "state": self.state,
            "retry_at_ms": self.retry_at_ms,
            "queued_at_ms": self.queued_at_ms,
            "running_at_ms": self.running_at_ms,
            "completed_at_ms": self.completed_at_ms,
            "failed_at_ms": self.failed_at_ms,
            "cancelled_at_ms": self.cancelled_at_ms,
            "stale_at_ms": self.stale_at_ms,
            "last_error": self.last_error,
            "updated_at_ms": self.updated_at_ms,
            "ready_for_verification": self.isReadyForVerification(),
            "queued_request_id": self.queued_request_id,
            "queued_priority": self.queued_priority,
            "queued_reason": self.queued_reason,
            "retry_count": self.retry_count,
        }


@dataclass(frozen=True, slots=True)
class PreviousDiscoveryState:
    pending_request: ExternalIedDiscoveryRequest | None = None
    lifecycle: DiscoveryStateMachine | None = None


@dataclass(frozen=True, slots=True)
class DiscoveryDecision:
    decision: DiscoveryDecisionKind
    reason: DiscoveryScheduleReason | None = None
    priority: DiscoveryPriority = "normal"
    earliest_execution_at_ms: int = 0
    dedupe: bool = True
    model_fingerprint: str | None = None
    planning_fingerprint: str | None = None


@dataclass(frozen=True, slots=True)
class ExternalIedDiscoveryRequest:
    request_id: str
    workspace_id: int
    endpoint: str
    ip: str
    port: int
    signal_ids: tuple[int, ...]
    reason: DiscoveryScheduleReason
    priority: DiscoveryPriority
    model_fingerprint: str | None
    planning_fingerprint: str | None
    requested_at_ms: int
    earliest_execution_at_ms: int
    dedupe: bool = True

    def to_payload(self) -> dict[str, Any]:
        return {
            "request_id": self.request_id,
            "workspace_id": self.workspace_id,
            "endpoint": self.endpoint,
            "ip": self.ip,
            "port": self.port,
            "signal_ids": list(self.signal_ids),
            "reason": self.reason,
            "priority": self.priority,
            "model_fingerprint": self.model_fingerprint,
            "planning_fingerprint": self.planning_fingerprint,
            "requested_at_ms": self.requested_at_ms,
            "earliest_execution_at_ms": self.earliest_execution_at_ms,
        }

    @classmethod
    def from_payload(cls, payload: dict[str, Any]) -> ExternalIedDiscoveryRequest:
        return cls(
            request_id=str(payload.get("request_id") or ""),
            workspace_id=int(payload.get("workspace_id") or 0),
            endpoint=str(payload.get("endpoint") or ""),
            ip=str(payload.get("ip") or ""),
            port=_normalize_port(payload.get("port")),
            signal_ids=_normalize_signal_ids(payload.get("signal_ids")),
            reason=_discovery_reason(payload.get("reason")),
            priority="high" if payload.get("priority") == "high" else "normal",
            model_fingerprint=_str_or_none(payload.get("model_fingerprint")),
            planning_fingerprint=_str_or_none(payload.get("planning_fingerprint")),
            requested_at_ms=_int_or_none(payload.get("requested_at_ms")) or int(time.time() * 1000),
            earliest_execution_at_ms=_int_or_none(payload.get("earliest_execution_at_ms")) or int(time.time() * 1000),
            dedupe=bool(payload.get("dedupe", True)),
        )


@dataclass(frozen=True, slots=True)
class ExternalIedDiscoverySchedulerConfig:
    dedupe_ttl_seconds: int = 300
    running_timeout_ms: int = 120000


class ExternalIedDiscoveryJobSink(Protocol):
    async def enqueue(self, request: ExternalIedDiscoveryRequest) -> str: ...


class DiscoveryPolicy:
    def evaluate(
        self,
        *,
        endpoint_state: EndpointState,
        cache_metadata: DiscoveryCacheMetadata | None = None,
        verification_context: VerificationContext | None = None,
        user_action: UserAction | None = None,
        previous_discovery_state: PreviousDiscoveryState | None = None,
        now_ms: int,
    ) -> DiscoveryDecision:
        verification = verification_context or VerificationContext()
        action = user_action or UserAction()
        previous = previous_discovery_state or PreviousDiscoveryState()
        lifecycle = previous.lifecycle or DiscoveryStateMachine()
        if endpoint_state.status == "not_applicable":
            return DiscoveryDecision(decision="Ignore")
        if not _is_discovery_allowed(endpoint_state):
            return DiscoveryDecision(decision="WaitForStableConnection")
        if action.action in {"refresh", "discover"}:
            return DiscoveryDecision(
                decision="QueueHighPriorityDiscovery",
                reason="user_refresh",
                priority="high",
                earliest_execution_at_ms=now_ms,
                dedupe=False,
                model_fingerprint=cache_metadata.model_fingerprint if cache_metadata is not None else None,
                planning_fingerprint=verification.planning_fingerprint or (cache_metadata.planning_fingerprint if cache_metadata is not None else None),
            )
        if verification.verification_required:
            return DiscoveryDecision(
                decision="QueueHighPriorityDiscovery",
                reason="verification_required",
                priority="high",
                earliest_execution_at_ms=now_ms,
                model_fingerprint=cache_metadata.model_fingerprint if cache_metadata is not None else None,
                planning_fingerprint=verification.planning_fingerprint or (cache_metadata.planning_fingerprint if cache_metadata is not None else None),
            )
        if not lifecycle.canSchedule(now_ms=now_ms):
            return DiscoveryDecision(decision="NoAction")
        if lifecycle.state == "RetryWaiting":
            priority = lifecycle.queued_priority or "normal"
            return DiscoveryDecision(
                decision="QueueHighPriorityDiscovery" if priority == "high" else "QueueDiscovery",
                reason=lifecycle.queued_reason or "cache_stale",
                priority=priority,
                earliest_execution_at_ms=now_ms,
                model_fingerprint=cache_metadata.model_fingerprint if cache_metadata is not None else None,
                planning_fingerprint=verification.planning_fingerprint or (cache_metadata.planning_fingerprint if cache_metadata is not None else None),
            )
        if cache_metadata is None:
            return DiscoveryDecision(
                decision="QueueDiscovery",
                reason="cache_missing",
                earliest_execution_at_ms=now_ms,
                planning_fingerprint=verification.planning_fingerprint,
            )
        if cache_metadata.discovery_version != EXTERNAL_IED_DISCOVERY_VERSION:
            return DiscoveryDecision(
                decision="QueueDiscovery",
                reason="cache_stale",
                earliest_execution_at_ms=now_ms,
                model_fingerprint=cache_metadata.model_fingerprint,
                planning_fingerprint=verification.planning_fingerprint or cache_metadata.planning_fingerprint,
            )
        if lifecycle.state == "Stale":
            return DiscoveryDecision(
                decision="QueueDiscovery",
                reason="cache_stale",
                earliest_execution_at_ms=now_ms,
                model_fingerprint=cache_metadata.model_fingerprint,
                planning_fingerprint=verification.planning_fingerprint or cache_metadata.planning_fingerprint,
            )
        if not cache_metadata.model_fingerprint:
            return DiscoveryDecision(
                decision="QueueDiscovery",
                reason="cache_stale",
                earliest_execution_at_ms=now_ms,
                planning_fingerprint=verification.planning_fingerprint or cache_metadata.planning_fingerprint,
            )
        if endpoint_state.priority_reason == REACHABLE_STABLE_REASON:
            return DiscoveryDecision(decision="NoAction")
        if endpoint_state.priority_reason == REACHABLE_CANDIDATE_REASON:
            if verification.verification_pending:
                return DiscoveryDecision(
                    decision="QueueHighPriorityDiscovery",
                    reason="reachable_candidate_verification_pending",
                    priority="high",
                    earliest_execution_at_ms=now_ms,
                    model_fingerprint=cache_metadata.model_fingerprint if cache_metadata is not None else None,
                    planning_fingerprint=verification.planning_fingerprint or (cache_metadata.planning_fingerprint if cache_metadata is not None else None),
                )
            return DiscoveryDecision(decision="WaitForStableConnection")
        return DiscoveryDecision(decision="NoAction")


class RedisExternalIedDiscoveryJobSink:
    async def enqueue(self, request: ExternalIedDiscoveryRequest) -> str:
        settings = get_settings()
        redis = RedisManager.get_instance()
        dedupe_key = _dedupe_key(request.workspace_id, request.endpoint)
        dedupe_written = False
        if request.dedupe:
            dedupe_payload = json.dumps(_dedupe_payload(request), separators=(",", ":"))
            raw_existing = await redis.get(dedupe_key)
            existing = _parse_payload(raw_existing)
            accepted = bool(
                existing is not None
                and (
                    _should_upgrade_pending(_request_stub_from_dedupe(existing), request.priority)
                    or _should_replace_pending_for_context(_request_stub_from_dedupe(existing), request)
                )
            )
            if accepted:
                await redis.set(dedupe_key, dedupe_payload, ex=settings.external_ied_discovery_job_dedupe_ttl_seconds)
            else:
                accepted = await redis.set(
                    dedupe_key,
                    dedupe_payload,
                    nx=True,
                    ex=settings.external_ied_discovery_job_dedupe_ttl_seconds,
                )
            if not accepted:
                return _str_or_none((existing or {}).get("request_id")) or str(raw_existing or "")
            dedupe_written = True
        try:
            entry_id = await redis.xadd(
                settings.external_ied_discovery_job_stream,
                {"data": json.dumps(request.to_payload(), separators=(",", ":"))},
                maxlen=settings.external_ied_discovery_job_stream_maxlen,
                approximate=True,
            )
            state = DiscoveryStateMachine()
            state.markQueued(
                now_ms=request.requested_at_ms,
                request_id=request.request_id,
                priority=request.priority,
                reason=request.reason,
            )
            await _store_external_ied_discovery_state(
                redis,
                workspace_id=request.workspace_id,
                endpoint=request.endpoint,
                state=state,
            )
            return entry_id
        except Exception:  # noqa: BLE001
            if dedupe_written:
                await redis.delete(dedupe_key)
            raise


class ExternalIedDiscoveryScheduler:
    def __init__(
        self,
        *,
        sink: ExternalIedDiscoveryJobSink,
        policy: DiscoveryPolicy | None = None,
        config: ExternalIedDiscoverySchedulerConfig | None = None,
        now_ms: Callable[[], int] | None = None,
    ) -> None:
        self._sink = sink
        self._policy = policy or DiscoveryPolicy()
        self._config = config or ExternalIedDiscoverySchedulerConfig()
        self._now_ms = now_ms or (lambda: int(time.time() * 1000))
        self._pending: dict[str, ExternalIedDiscoveryRequest] = {}
        self._discovery_states: dict[str, DiscoveryStateMachine] = {}

    @property
    def pending(self) -> dict[str, ExternalIedDiscoveryRequest]:
        return dict(self._pending)

    @property
    def discovery_states(self) -> dict[str, DiscoveryStateMachine]:
        return dict(self._discovery_states)

    async def on_watcher_state(
        self,
        endpoint_state: ExternalIedDiscoveryEndpointState,
        *,
        cache_record: ExternalIedDiscoveryCacheRecord | None = None,
        verification_pending: bool = False,
        planning_fingerprint: str | None = None,
        previous_discovery_state: PreviousDiscoveryState | None = None,
    ) -> ExternalIedDiscoveryRequest | None:
        return await self.evaluate_and_schedule(
            endpoint_state,
            cache_metadata=cache_record,
            verification_context=VerificationContext(
                verification_pending=verification_pending,
                planning_fingerprint=planning_fingerprint,
            ),
            previous_discovery_state=previous_discovery_state,
        )

    async def schedule_for_cache_if_needed(
        self,
        endpoint_state: ExternalIedDiscoveryEndpointState,
        *,
        cache_record: ExternalIedDiscoveryCacheRecord | None,
        planning_fingerprint: str | None = None,
        previous_discovery_state: PreviousDiscoveryState | None = None,
    ) -> ExternalIedDiscoveryRequest | None:
        return await self.evaluate_and_schedule(
            endpoint_state,
            cache_metadata=cache_record,
            verification_context=VerificationContext(planning_fingerprint=planning_fingerprint),
            previous_discovery_state=previous_discovery_state,
        )

    async def schedule_user_refresh(
        self,
        endpoint_state: ExternalIedDiscoveryEndpointState,
        *,
        previous_discovery_state: PreviousDiscoveryState | None = None,
    ) -> ExternalIedDiscoveryRequest | None:
        return await self.evaluate_and_schedule(
            endpoint_state,
            user_action=UserAction(action="refresh"),
            previous_discovery_state=previous_discovery_state,
        )

    async def schedule_verification_required(
        self,
        endpoint_state: ExternalIedDiscoveryEndpointState,
        *,
        planning_fingerprint: str | None = None,
        previous_discovery_state: PreviousDiscoveryState | None = None,
    ) -> ExternalIedDiscoveryRequest | None:
        return await self.evaluate_and_schedule(
            endpoint_state,
            verification_context=VerificationContext(
                verification_required=True,
                planning_fingerprint=planning_fingerprint,
            ),
            previous_discovery_state=previous_discovery_state,
        )

    async def evaluate_and_schedule(
        self,
        endpoint_state: EndpointState,
        *,
        cache_metadata: DiscoveryCacheMetadata | None = None,
        verification_context: VerificationContext | None = None,
        user_action: UserAction | None = None,
        previous_discovery_state: PreviousDiscoveryState | None = None,
    ) -> ExternalIedDiscoveryRequest | None:
        now_ms = self._now_ms()
        endpoint_key = _pending_key(endpoint_state)
        lifecycle = (
            previous_discovery_state.lifecycle
            if previous_discovery_state is not None and previous_discovery_state.lifecycle is not None
            else self._discovery_states.get(endpoint_key)
        )
        recovered_lifecycle = _recover_timed_out_running(lifecycle, now_ms=now_ms, timeout_ms=self._config.running_timeout_ms)
        if recovered_lifecycle is not lifecycle and recovered_lifecycle is not None:
            self._discovery_states[endpoint_key] = recovered_lifecycle
        lifecycle = recovered_lifecycle
        previous = PreviousDiscoveryState(
            pending_request=(
                previous_discovery_state.pending_request
                if previous_discovery_state is not None and previous_discovery_state.pending_request is not None
                else self._pending.get(endpoint_key)
            ),
            lifecycle=lifecycle,
        )
        decision = self._policy.evaluate(
            endpoint_state=endpoint_state,
            cache_metadata=cache_metadata,
            verification_context=verification_context,
            user_action=user_action,
            previous_discovery_state=previous,
            now_ms=now_ms,
        )
        if decision.decision not in {"QueueDiscovery", "QueueHighPriorityDiscovery"}:
            return None
        if decision.reason is None or decision.earliest_execution_at_ms > now_ms:
            return None
        return await self._enqueue(endpoint_state, decision=decision, now_ms=now_ms, lifecycle=lifecycle)

    async def _enqueue(
        self,
        endpoint_state: ExternalIedDiscoveryEndpointState,
        *,
        decision: DiscoveryDecision,
        now_ms: int,
        lifecycle: DiscoveryStateMachine | None = None,
    ) -> ExternalIedDiscoveryRequest | None:
        endpoint_key = _pending_key(endpoint_state)
        existing = self._pending.get(endpoint_key)
        current_lifecycle = lifecycle or self._discovery_states.get(endpoint_key) or DiscoveryStateMachine()
        upgrade_pending = existing is not None and _should_upgrade_pending(existing, decision.priority)
        replace_for_context = (
            existing is not None
            and decision.priority == "high"
            and decision.reason == "verification_required"
            and bool(decision.planning_fingerprint)
            and existing.planning_fingerprint != decision.planning_fingerprint
        )
        state_upgrade = existing is None and _should_upgrade_state(current_lifecycle, decision.priority)
        manual_replace = not decision.dedupe and current_lifecycle.state == "Queued"
        if not current_lifecycle.canSchedule(now_ms=now_ms) and not upgrade_pending and not replace_for_context and not state_upgrade and not manual_replace:
            return None
        dedupe_window_ms = max(0, int(self._config.dedupe_ttl_seconds) * 1000)
        if (
            decision.dedupe
            and existing is not None
            and now_ms - existing.requested_at_ms <= dedupe_window_ms
            and not upgrade_pending
            and not replace_for_context
        ):
            return None
        if upgrade_pending or replace_for_context or state_upgrade or manual_replace:
            current_lifecycle = DiscoveryStateMachine()

        request = ExternalIedDiscoveryRequest(
            request_id=uuid4().hex,
            workspace_id=endpoint_state.workspace_id,
            endpoint=endpoint_state.endpoint,
            ip=endpoint_state.ip,
            port=endpoint_state.port,
            signal_ids=endpoint_state.signal_ids,
            reason=decision.reason,
            priority=decision.priority,
            model_fingerprint=decision.model_fingerprint,
            planning_fingerprint=decision.planning_fingerprint,
            requested_at_ms=now_ms,
            earliest_execution_at_ms=decision.earliest_execution_at_ms,
            dedupe=decision.dedupe,
        )
        await self._sink.enqueue(request)
        current_lifecycle.markQueued(
            now_ms=now_ms,
            request_id=request.request_id,
            priority=request.priority,
            reason=request.reason,
        )
        self._discovery_states[endpoint_key] = current_lifecycle
        self._pending[endpoint_key] = request
        return request


def endpoint_state_from_status_payload(
    *,
    workspace_id: int,
    endpoint: str,
    payload: str | dict[str, Any] | None,
) -> ExternalIedDiscoveryEndpointState | None:
    parsed = _parse_payload(payload)
    if parsed is None:
        return None
    ip = _normalize_ip(parsed.get("ip")) or _normalize_ip(endpoint.rsplit(":", 1)[0])
    if not ip:
        return None
    return ExternalIedDiscoveryEndpointState(
        workspace_id=workspace_id,
        ip=ip,
        port=_normalize_port(parsed.get("port")),
        status=str(parsed.get("status") or "expected"),
        signal_ids=_normalize_signal_ids(parsed.get("signal_ids")),
        priority_reason=str(parsed.get("priority_reason") or ""),
        failure_code=parsed.get("failure_code") if isinstance(parsed.get("failure_code"), str) else None,
        last_checked_at=parsed.get("last_checked_at") if isinstance(parsed.get("last_checked_at"), str) else None,
    )


def endpoint_state_from_record(
    *,
    workspace_id: int,
    record: ExternalIedStatusRecord,
    priority_reason: str | None = None,
) -> ExternalIedDiscoveryEndpointState:
    return ExternalIedDiscoveryEndpointState(
        workspace_id=workspace_id,
        ip=record.ip,
        port=record.port,
        status=record.status,
        signal_ids=tuple(sorted(set(int(signal_id) for signal_id in record.signal_ids if int(signal_id) > 0))),
        priority_reason=priority_reason,
        failure_code=record.failure_code,
        last_checked_at=record.last_checked_at,
    )


async def schedule_external_ied_discovery_from_watcher_payload(
    *,
    workspace_id: int,
    endpoint: str,
    payload: str | dict[str, Any],
    verification_pending: bool = False,
) -> ExternalIedDiscoveryRequest | None:
    endpoint_state = endpoint_state_from_status_payload(workspace_id=workspace_id, endpoint=endpoint, payload=payload)
    if endpoint_state is None:
        return None
    cache_record = await load_external_ied_discovery_cache(workspace_id=workspace_id, endpoint=endpoint_state.endpoint)
    discovery_state = await recover_external_ied_discovery_running_timeout(workspace_id=workspace_id, endpoint=endpoint_state.endpoint)
    return await _redis_scheduler().on_watcher_state(
        endpoint_state,
        cache_record=cache_record,
        verification_pending=verification_pending,
        previous_discovery_state=PreviousDiscoveryState(lifecycle=discovery_state),
    )


async def schedule_external_ied_discovery_for_user_request(
    *,
    workspace_id: int,
    endpoint: str,
) -> ExternalIedDiscoveryRequest | None:
    endpoint_state = await load_external_ied_state(workspace_id=workspace_id, endpoint=endpoint)
    if endpoint_state is None:
        return None
    discovery_state = await recover_external_ied_discovery_running_timeout(workspace_id=workspace_id, endpoint=endpoint_state.endpoint)
    return await _redis_scheduler().schedule_user_refresh(
        endpoint_state,
        previous_discovery_state=PreviousDiscoveryState(lifecycle=discovery_state),
    )


async def schedule_external_ied_discovery_for_verification(
    *,
    workspace_id: int,
    endpoint: str,
) -> ExternalIedDiscoveryRequest | None:
    endpoint_state = await load_external_ied_state(workspace_id=workspace_id, endpoint=endpoint)
    if endpoint_state is None:
        return None
    discovery_state = await recover_external_ied_discovery_running_timeout(workspace_id=workspace_id, endpoint=endpoint_state.endpoint)
    return await _redis_scheduler().schedule_verification_required(
        endpoint_state,
        previous_discovery_state=PreviousDiscoveryState(lifecycle=discovery_state),
    )


async def load_external_ied_state(*, workspace_id: int, endpoint: str) -> ExternalIedDiscoveryEndpointState | None:
    redis = RedisManager.get_instance()
    payload = await redis.hget(_status_key(workspace_id), endpoint)
    return endpoint_state_from_status_payload(workspace_id=workspace_id, endpoint=endpoint, payload=payload)


async def load_external_ied_discovery_cache(
    *,
    workspace_id: int,
    endpoint: str,
) -> ExternalIedDiscoveryCacheRecord | None:
    redis = RedisManager.get_instance()
    payload = await redis.hget(_cache_key(workspace_id), endpoint)
    parsed = _parse_payload(payload)
    if parsed is None:
        return None
    return discovery_cache_metadata_from_payload(parsed)


async def load_external_ied_discovery_state(
    *,
    workspace_id: int,
    endpoint: str,
) -> DiscoveryStateMachine:
    redis = RedisManager.get_instance()
    payload = await redis.hget(_discovery_state_key(workspace_id), endpoint)
    return discovery_state_from_payload(payload)


async def recover_external_ied_discovery_running_timeout(
    *,
    workspace_id: int,
    endpoint: str,
    now_ms: int | None = None,
    timeout_ms: int | None = None,
) -> DiscoveryStateMachine:
    settings = get_settings()
    checked_at_ms = now_ms or int(time.time() * 1000)
    state = await load_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint)
    recovered = _recover_timed_out_running(
        state,
        now_ms=checked_at_ms,
        timeout_ms=timeout_ms if timeout_ms is not None else settings.external_ied_discovery_running_timeout_ms,
    )
    if recovered is not state:
        await record_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint, state=recovered)
        redis = RedisManager.get_instance()
        await redis.delete(_dedupe_key(workspace_id, endpoint))
    return recovered


async def mark_external_ied_discovery_running(
    *,
    workspace_id: int,
    endpoint: str,
    request_id: str | None = None,
    now_ms: int | None = None,
) -> DiscoveryStateMachine:
    state = await load_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint)
    started_at_ms = now_ms or int(time.time() * 1000)
    if state.state == "NeverDiscovered":
        state.markQueued(now_ms=started_at_ms)
    if state.queued_request_id and request_id and state.queued_request_id != request_id:
        raise ValueError("Stale external IED discovery request was replaced by a newer queued request.")
    state.markRunning(now_ms=started_at_ms)
    await record_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint, state=state)
    return state


async def mark_external_ied_discovery_failed(
    *,
    workspace_id: int,
    endpoint: str,
    error: str | None = None,
    now_ms: int | None = None,
) -> DiscoveryStateMachine:
    state = await load_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint)
    state.markFailed(now_ms=now_ms or int(time.time() * 1000), error=error)
    await record_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint, state=state)
    return state


async def mark_external_ied_discovery_retry(
    *,
    workspace_id: int,
    endpoint: str,
    retry_at_ms: int,
    error: str | None = None,
    now_ms: int | None = None,
) -> DiscoveryStateMachine:
    state = await load_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint)
    state.markRetry(now_ms=now_ms or int(time.time() * 1000), retry_at_ms=retry_at_ms, error=error)
    await record_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint, state=state)
    return state


async def mark_external_ied_discovery_cancelled(
    *,
    workspace_id: int,
    endpoint: str,
    now_ms: int | None = None,
) -> DiscoveryStateMachine:
    state = await load_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint)
    state.markCancelled(now_ms=now_ms or int(time.time() * 1000))
    await record_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint, state=state)
    return state


async def record_external_ied_discovery_state(
    *,
    workspace_id: int,
    endpoint: str,
    state: DiscoveryStateMachine,
) -> None:
    redis = RedisManager.get_instance()
    await _store_external_ied_discovery_state(redis, workspace_id=workspace_id, endpoint=endpoint, state=state)


async def record_external_ied_discovery_result(
    *,
    workspace_id: int,
    endpoint: str,
    metadata: DiscoveryCacheMetadata,
    model: dict[str, Any],
) -> DiscoveryCacheMetadata:
    now_ms = metadata.last_successful_discovery_at_ms or metadata.last_discovery_at_ms or int(time.time() * 1000)
    state = await load_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint)
    state.markCompleted(now_ms=now_ms)
    redis = RedisManager.get_instance()
    await redis.hset(_cache_key(workspace_id), endpoint, json.dumps(metadata.to_payload(), separators=(",", ":")))
    await redis.hset(_discovery_model_key(workspace_id), endpoint, json.dumps(model, separators=(",", ":"), sort_keys=True))
    await _store_external_ied_discovery_state(redis, workspace_id=workspace_id, endpoint=endpoint, state=state)
    await redis.delete(_dedupe_key(workspace_id, endpoint))
    return metadata


async def emit_external_ied_discovery_completed(
    *,
    workspace_id: int,
    endpoint: str,
    request: ExternalIedDiscoveryRequest,
    metadata: DiscoveryCacheMetadata,
) -> str:
    settings = get_settings()
    redis = RedisManager.get_instance()
    payload = {
        "event": "ExternalIedDiscoveryCompleted",
        "workspace_id": workspace_id,
        "endpoint": endpoint,
        "ip": request.ip,
        "port": request.port,
        "signal_ids": list(request.signal_ids),
        "model_fingerprint": metadata.model_fingerprint,
        "planning_fingerprint": metadata.planning_fingerprint,
        "completed_at_ms": metadata.last_successful_discovery_at_ms or metadata.last_discovery_at_ms,
    }
    return await redis.xadd(
        settings.external_ied_planning_event_stream,
        {"data": json.dumps(payload, separators=(",", ":"))},
        maxlen=settings.external_ied_planning_event_stream_maxlen,
        approximate=True,
    )


async def record_external_ied_discovery_failure(
    *,
    workspace_id: int,
    endpoint: str,
    error: str | None = None,
    duration_ms: int | None = None,
    now_ms: int | None = None,
) -> DiscoveryCacheMetadata:
    failed_at_ms = now_ms or int(time.time() * 1000)
    settings = get_settings()
    previous = await load_external_ied_discovery_cache(workspace_id=workspace_id, endpoint=endpoint)
    metadata = DiscoveryCacheMetadata(
        discovery_version=(previous.discovery_version if previous is not None else EXTERNAL_IED_DISCOVERY_VERSION),
        device_identity=previous.device_identity if previous is not None else None,
        vendor=previous.vendor if previous is not None else None,
        model=previous.model if previous is not None else None,
        config_rev=previous.config_rev if previous is not None else None,
        last_discovery_at_ms=failed_at_ms,
        last_successful_discovery_at_ms=previous.last_successful_discovery_at_ms if previous is not None else None,
        last_failed_discovery_at_ms=failed_at_ms,
        last_error=error,
        discovery_duration_ms=duration_ms,
        model_fingerprint=previous.model_fingerprint if previous is not None else None,
        planning_fingerprint=previous.planning_fingerprint if previous is not None else None,
    )
    state = await load_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint)
    if state.state == "NeverDiscovered":
        state.markQueued(now_ms=failed_at_ms)
    if state.state == "Queued":
        state.markRunning(now_ms=failed_at_ms)
    if is_transient_discovery_error(error):
        retry_delay_ms = min(
            max(1, settings.external_ied_discovery_retry_base_ms) * (2 ** max(0, state.retry_count)),
            max(settings.external_ied_discovery_retry_base_ms, settings.external_ied_discovery_retry_max_ms),
        )
        state.markRetry(now_ms=failed_at_ms, retry_at_ms=failed_at_ms + retry_delay_ms, error=error)
    else:
        state.markFailed(now_ms=failed_at_ms, error=error)
    redis = RedisManager.get_instance()
    await redis.hset(_cache_key(workspace_id), endpoint, json.dumps(metadata.to_payload(), separators=(",", ":")))
    await _store_external_ied_discovery_state(redis, workspace_id=workspace_id, endpoint=endpoint, state=state)
    await redis.delete(_dedupe_key(workspace_id, endpoint))
    return metadata


async def record_external_ied_discovery_cache(
    *,
    workspace_id: int,
    endpoint_state: ExternalIedDiscoveryEndpointState,
    metadata: DiscoveryCacheMetadata,
) -> ExternalIedDiscoveryCacheRecord:
    record = metadata
    completed_at_ms = record.last_successful_discovery_at_ms or record.last_discovery_at_ms or int(time.time() * 1000)
    state = await load_external_ied_discovery_state(workspace_id=workspace_id, endpoint=endpoint_state.endpoint)
    state.markCompleted(now_ms=completed_at_ms)
    redis = RedisManager.get_instance()
    await redis.hset(
        _cache_key(workspace_id),
        endpoint_state.endpoint,
        json.dumps(record.to_payload(), separators=(",", ":")),
    )
    await _store_external_ied_discovery_state(
        redis,
        workspace_id=workspace_id,
        endpoint=endpoint_state.endpoint,
        state=state,
    )
    await redis.delete(_dedupe_key(workspace_id, endpoint_state.endpoint))
    return record


def discovery_cache_metadata_from_payload(payload: str | dict[str, Any] | None) -> DiscoveryCacheMetadata | None:
    parsed = _parse_payload(payload)
    if parsed is None:
        return None
    return DiscoveryCacheMetadata(
        discovery_version=_str_or_none(parsed.get("discovery_version")),
        device_identity=_str_or_none(parsed.get("device_identity")),
        vendor=_str_or_none(parsed.get("vendor")),
        model=_str_or_none(parsed.get("model")),
        config_rev=_str_or_none(parsed.get("configRev") if "configRev" in parsed else parsed.get("config_rev")),
        last_discovery_at_ms=_int_or_none(parsed.get("last_discovery_at_ms")),
        last_successful_discovery_at_ms=_int_or_none(parsed.get("last_successful_discovery_at_ms")),
        last_failed_discovery_at_ms=_int_or_none(parsed.get("last_failed_discovery_at_ms")),
        last_error=_str_or_none(parsed.get("last_error")),
        discovery_duration_ms=_int_or_none(parsed.get("discovery_duration_ms")),
        model_fingerprint=_str_or_none(parsed.get("model_fingerprint")),
        planning_fingerprint=_str_or_none(parsed.get("planning_fingerprint")),
    )


def discovery_state_from_payload(payload: str | dict[str, Any] | None) -> DiscoveryStateMachine:
    parsed = _parse_payload(payload)
    if parsed is None:
        return DiscoveryStateMachine()
    state = parsed.get("state")
    if state not in {"NeverDiscovered", "Queued", "Running", "Succeeded", "Failed", "RetryWaiting", "Cancelled", "Stale"}:
        state = "NeverDiscovered"
    return DiscoveryStateMachine(
        state=state,
        retry_at_ms=_int_or_none(parsed.get("retry_at_ms")),
        queued_at_ms=_int_or_none(parsed.get("queued_at_ms")),
        running_at_ms=_int_or_none(parsed.get("running_at_ms")),
        completed_at_ms=_int_or_none(parsed.get("completed_at_ms")),
        failed_at_ms=_int_or_none(parsed.get("failed_at_ms")),
        cancelled_at_ms=_int_or_none(parsed.get("cancelled_at_ms")),
        stale_at_ms=_int_or_none(parsed.get("stale_at_ms")),
        last_error=parsed.get("last_error") if isinstance(parsed.get("last_error"), str) else None,
        updated_at_ms=_int_or_none(parsed.get("updated_at_ms")),
        queued_request_id=_str_or_none(parsed.get("queued_request_id")),
        queued_priority="high" if parsed.get("queued_priority") == "high" else ("normal" if parsed.get("queued_priority") == "normal" else None),
        queued_reason=_discovery_reason_or_none(parsed.get("queued_reason")),
        retry_count=max(0, _int_or_none(parsed.get("retry_count")) or 0),
    )


def discovery_ui_fields_from_payload(payload: str | dict[str, Any] | None) -> dict[str, Any]:
    state = discovery_state_from_payload(payload)
    return {
        "discovery_state": state.state,
        "discovery_retry_at_ms": state.retry_at_ms,
        "discovery_last_error": state.last_error,
        "discovery_updated_at_ms": state.updated_at_ms,
        "discovery_ready_for_verification": state.isReadyForVerification(),
    }


def _is_discovery_allowed(endpoint_state: ExternalIedDiscoveryEndpointState) -> bool:
    if endpoint_state.status in BLOCKED_STATUSES:
        return False
    if endpoint_state.priority_reason == SUSPECT_OFFLINE_REASON:
        return False
    if endpoint_state.failure_code in BLOCKED_FAILURE_CODES:
        return False
    return endpoint_state.status == "reachable"


def _redis_scheduler() -> ExternalIedDiscoveryScheduler:
    global _REDIS_SCHEDULER
    if _REDIS_SCHEDULER is None:
        settings = get_settings()
        _REDIS_SCHEDULER = ExternalIedDiscoveryScheduler(
            sink=RedisExternalIedDiscoveryJobSink(),
            config=ExternalIedDiscoverySchedulerConfig(
                dedupe_ttl_seconds=settings.external_ied_discovery_job_dedupe_ttl_seconds,
                running_timeout_ms=settings.external_ied_discovery_running_timeout_ms,
            ),
        )
    return _REDIS_SCHEDULER


def _pending_key(endpoint_state: ExternalIedDiscoveryEndpointState) -> str:
    return f"{endpoint_state.workspace_id}:{endpoint_state.endpoint}"


def _status_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:status"


def _cache_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:discovery_cache"


def _discovery_state_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:discovery_state"


def _discovery_model_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:discovery_model"


def _dedupe_key(workspace_id: int, endpoint: str) -> str:
    return f"external_ied:workspace:{workspace_id}:discovery_pending:{endpoint}"


def _dedupe_payload(request: ExternalIedDiscoveryRequest) -> dict[str, Any]:
    return {
        "request_id": request.request_id,
        "priority": request.priority,
        "reason": request.reason,
        "planning_fingerprint": request.planning_fingerprint,
        "requested_at_ms": request.requested_at_ms,
    }


def _request_stub_from_dedupe(payload: dict[str, Any]) -> ExternalIedDiscoveryRequest:
    return ExternalIedDiscoveryRequest(
        request_id=str(payload.get("request_id") or ""),
        workspace_id=0,
        endpoint="",
        ip="",
        port=102,
        signal_ids=(),
        reason=_discovery_reason(payload.get("reason")),
        priority="high" if payload.get("priority") == "high" else "normal",
        model_fingerprint=None,
        planning_fingerprint=_str_or_none(payload.get("planning_fingerprint")),
        requested_at_ms=_int_or_none(payload.get("requested_at_ms")) or 0,
        earliest_execution_at_ms=0,
    )


def _should_upgrade_pending(existing: ExternalIedDiscoveryRequest, new_priority: DiscoveryPriority) -> bool:
    return existing.priority == "normal" and new_priority == "high"


def _should_replace_pending_for_context(existing: ExternalIedDiscoveryRequest, request: ExternalIedDiscoveryRequest) -> bool:
    if request.priority != "high":
        return False
    if request.reason != "verification_required":
        return False
    return bool(request.planning_fingerprint and existing.planning_fingerprint != request.planning_fingerprint)


def _should_upgrade_state(state: DiscoveryStateMachine, new_priority: DiscoveryPriority) -> bool:
    return state.state == "Queued" and state.queued_priority == "normal" and new_priority == "high"


def _recover_timed_out_running(
    state: DiscoveryStateMachine | None,
    *,
    now_ms: int,
    timeout_ms: int,
) -> DiscoveryStateMachine | None:
    if state is None or state.state != "Running" or state.running_at_ms is None:
        return state
    if now_ms - state.running_at_ms < max(1, timeout_ms):
        return state
    recovered = DiscoveryStateMachine(
        state=state.state,
        retry_at_ms=state.retry_at_ms,
        queued_at_ms=state.queued_at_ms,
        running_at_ms=state.running_at_ms,
        completed_at_ms=state.completed_at_ms,
        failed_at_ms=state.failed_at_ms,
        cancelled_at_ms=state.cancelled_at_ms,
        stale_at_ms=state.stale_at_ms,
        last_error=state.last_error,
        updated_at_ms=state.updated_at_ms,
        queued_request_id=state.queued_request_id,
        queued_priority=state.queued_priority,
        queued_reason=state.queued_reason,
        retry_count=state.retry_count,
    )
    retry_delay_ms = min(max(1, get_settings().external_ied_discovery_retry_base_ms), max(1, timeout_ms))
    recovered.markRetry(
        now_ms=now_ms,
        retry_at_ms=now_ms + retry_delay_ms,
        error="Discovery worker timed out while Running.",
    )
    return recovered


def is_transient_discovery_error(error: str | None) -> bool:
    text = (error or "").lower()
    if not text:
        return False
    permanent_tokens = ("parse", "schema", "invalid discovery model", "model build", "unsupported")
    if any(token in text for token in permanent_tokens):
        return False
    transient_tokens = (
        "timeout",
        "timed out",
        "connection",
        "network",
        "unreachable",
        "refused",
        "reset",
        "route",
        "temporary",
        "mms association",
    )
    return any(token in text for token in transient_tokens)


async def _store_external_ied_discovery_state(
    redis,
    *,
    workspace_id: int,
    endpoint: str,
    state: DiscoveryStateMachine,
) -> None:
    await redis.hset(
        _discovery_state_key(workspace_id),
        endpoint,
        json.dumps(state.to_payload(), separators=(",", ":")),
    )


def _parse_payload(payload: str | dict[str, Any] | None) -> dict[str, Any] | None:
    if isinstance(payload, dict):
        return payload
    if not payload:
        return None
    try:
        parsed = json.loads(payload)
    except json.JSONDecodeError:
        return None
    return parsed if isinstance(parsed, dict) else None


def _discovery_reason_or_none(value: Any) -> DiscoveryScheduleReason | None:
    if value in {
        "reachable_stable",
        "reachable_candidate_verification_pending",
        "user_refresh",
        "cache_missing",
        "cache_stale",
        "verification_required",
    }:
        return value
    return None


def _normalize_ip(value: Any) -> str | None:
    text = str(value or "").strip()
    parts = text.split(".")
    if len(parts) != 4:
        return None
    try:
        octets = [int(part, 10) for part in parts]
    except ValueError:
        return None
    if any(octet < 0 or octet > 255 for octet in octets):
        return None
    return ".".join(str(octet) for octet in octets)


def _normalize_port(value: Any) -> int:
    try:
        port = int(value)
    except (TypeError, ValueError):
        return 102
    return port if 1 <= port <= 65535 else 102


def _normalize_signal_ids(values: Any) -> tuple[int, ...]:
    if not isinstance(values, list | tuple):
        return ()
    result: list[int] = []
    seen: set[int] = set()
    for value in values:
        try:
            signal_id = int(value)
        except (TypeError, ValueError):
            continue
        if signal_id <= 0 or signal_id in seen:
            continue
        seen.add(signal_id)
        result.append(signal_id)
    return tuple(sorted(result))


def _int_or_none(value: Any) -> int | None:
    try:
        parsed = int(value)
    except (TypeError, ValueError):
        return None
    return parsed if parsed >= 0 else None


def _str_or_none(value: Any) -> str | None:
    if not isinstance(value, str):
        return None
    stripped = value.strip()
    return stripped or None


def _discovery_reason(value: Any) -> DiscoveryScheduleReason:
    if value in {
        "reachable_stable",
        "reachable_candidate_verification_pending",
        "user_refresh",
        "cache_missing",
        "cache_stale",
        "verification_required",
    }:
        return value
    return "cache_missing"
