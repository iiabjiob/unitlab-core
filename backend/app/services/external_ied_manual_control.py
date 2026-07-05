from __future__ import annotations

from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
import json
import logging
import re
from threading import Event, RLock, Thread
from typing import Any, Callable
from uuid import uuid4

from redis import Redis

from app.core.config import get_settings
from app.schemas.ws.events import ExternalIedManualReportValuesChangedEvent
from app.services.iec61850.client_control import Iec61850ClientControlService
from app.services.iec61850.report_runtime import (
    Iec61850DataSetMember,
    Iec61850DeviceEndpoint,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportKind,
    Iec61850RuntimeMode,
    Iec61850RuntimeTriggerOptions,
)

_MANUAL_INSPECTOR_OWNER = "unitlab-manual-ied-inspector"
_DEFAULT_LEASE_TTL_SECONDS = 15.0
_DEFAULT_CLEANUP_INTERVAL_SECONDS = 2.0
_DEFAULT_REPORT_POLL_INTERVAL_SECONDS = 0.5
_logger = logging.getLogger(__name__)
_settings = get_settings()
_sync_redis: Redis | None = None


@dataclass(frozen=True, slots=True)
class ExternalIedManualReportRequest:
    workspace_id: int
    endpoint: str
    report_reference: str
    report_name: str | None = None
    report_kind: str | None = None
    dataset_reference: str | None = None


@dataclass(frozen=True, slots=True)
class ExternalIedManualReportResult:
    workspace_id: int
    endpoint: str
    report_reference: str
    enabled: bool
    status: str
    message: str | None = None
    lease_id: str | None = None
    owner: str | None = None
    created_at: str | None = None
    renewed_at: str | None = None
    expires_at: str | None = None
    signal_states: tuple[dict[str, Any], ...] = ()
    report_values: tuple[dict[str, Any], ...] = ()


@dataclass(slots=True)
class _ExternalIedManualReportLease:
    lease_id: str
    owner: str
    workspace_id: int
    endpoint: str
    report_reference: str
    session_key: str
    created_at: datetime
    renewed_at: datetime
    expires_at: datetime

    def renew(self, ttl: timedelta) -> None:
        now = _utc_now()
        self.renewed_at = now
        self.expires_at = now + ttl


class ExternalIedManualReportControlService:
    def __init__(
        self,
        *,
        control_service_factory: Callable[..., Iec61850ClientControlService] = Iec61850ClientControlService,
        event_publisher: Callable[[dict[str, Any]], None] | None = None,
        lease_ttl_seconds: float = _DEFAULT_LEASE_TTL_SECONDS,
        cleanup_interval_seconds: float = _DEFAULT_CLEANUP_INTERVAL_SECONDS,
        report_poll_interval_seconds: float = _DEFAULT_REPORT_POLL_INTERVAL_SECONDS,
        start_cleanup_thread: bool = True,
    ) -> None:
        self._control_service_factory = control_service_factory
        self._event_publisher = event_publisher or _publish_ws_event_sync
        self._lease_ttl = timedelta(seconds=max(float(lease_ttl_seconds), 0.05))
        self._cleanup_interval_seconds = max(float(cleanup_interval_seconds), 0.25)
        self._report_poll_interval_seconds = max(float(report_poll_interval_seconds), 0.1)
        self._sessions: dict[str, Iec61850ClientControlService] = {}
        self._leases_by_session: dict[str, _ExternalIedManualReportLease] = {}
        self._session_by_lease_id: dict[str, str] = {}
        self._last_published_report_signatures: dict[str, str] = {}
        self._lock = RLock()
        self._stop_cleanup = Event()
        self._cleanup_thread: Thread | None = None
        self._report_poll_thread: Thread | None = None
        if start_cleanup_thread:
            self._cleanup_thread = Thread(
                target=self._cleanup_expired_leases_loop,
                name="external-ied-manual-lease-cleanup",
                daemon=True,
            )
            self._cleanup_thread.start()
            self._report_poll_thread = Thread(
                target=self._poll_manual_report_values_loop,
                name="external-ied-manual-report-poll",
                daemon=True,
            )
            self._report_poll_thread.start()

    def shutdown(self) -> None:
        self._stop_cleanup.set()
        if self._cleanup_thread is not None:
            self._cleanup_thread.join(timeout=2.0)
        if self._report_poll_thread is not None:
            self._report_poll_thread.join(timeout=2.0)

    def set_report_enabled(
        self,
        request: ExternalIedManualReportRequest,
        *,
        enabled: bool,
    ) -> ExternalIedManualReportResult:
        endpoint = _parse_endpoint(request.endpoint)
        candidate = _candidate_from_report_request(request)
        session_key = _session_key(request.workspace_id, endpoint, candidate.id)
        with self._lock:
            if enabled:
                service = self._sessions.get(session_key)
                if service is None:
                    service = self._control_service_factory(
                        session_id=f"external-ied-manual:{request.workspace_id}:{endpoint.host}:{endpoint.port}:{len(self._sessions)}",
                        client_id=_MANUAL_INSPECTOR_OWNER,
                        endpoint=endpoint,
                        candidate=candidate,
                        available_candidates=(candidate,),
                    )
                    self._sessions[session_key] = service
                snapshot = service.enable_reporting()
                message = snapshot.last_diagnostic.message if snapshot.last_diagnostic else None
                lease = self._leases_by_session.get(session_key)
                if lease is None:
                    now = _utc_now()
                    lease = _ExternalIedManualReportLease(
                        lease_id=uuid4().hex,
                        owner=_MANUAL_INSPECTOR_OWNER,
                        workspace_id=request.workspace_id,
                        endpoint=request.endpoint,
                        report_reference=request.report_reference,
                        session_key=session_key,
                        created_at=now,
                        renewed_at=now,
                        expires_at=now + self._lease_ttl,
                    )
                    self._leases_by_session[session_key] = lease
                    self._session_by_lease_id[lease.lease_id] = session_key
                else:
                    lease.renew(self._lease_ttl)
                return ExternalIedManualReportResult(
                    workspace_id=request.workspace_id,
                    endpoint=request.endpoint,
                    report_reference=request.report_reference,
                    enabled=True,
                    status=str(snapshot.last_state.runtime_status.value if snapshot.last_state else "enabled"),
                    message=message,
                    signal_states=_snapshot_signal_states(snapshot),
                    report_values=_snapshot_report_values(snapshot),
                    **_lease_fields(lease),
                )

            return self._release_session_locked(
                session_key,
                workspace_id=request.workspace_id,
                endpoint=request.endpoint,
                report_reference=request.report_reference,
                reason="manual-release",
            )

    def renew_lease(
        self,
        *,
        workspace_id: int,
        endpoint: str,
        lease_id: str,
    ) -> ExternalIedManualReportResult:
        with self._lock:
            session_key = self._session_by_lease_id.get(lease_id)
            if session_key is None:
                raise ValueError("External IED manual report lease was not found.")
            lease = self._leases_by_session.get(session_key)
            if lease is None or lease.workspace_id != workspace_id or lease.endpoint != endpoint:
                raise ValueError("External IED manual report lease does not match this endpoint.")
            service = self._sessions.get(session_key)
            if service is None or _service_report_enabled(service) is False:
                return self._release_session_locked(
                    session_key,
                    workspace_id=lease.workspace_id,
                    endpoint=lease.endpoint,
                    report_reference=lease.report_reference,
                    reason="report-disabled",
                )
            if lease.expires_at <= _utc_now():
                return self._release_session_locked(
                    session_key,
                    workspace_id=lease.workspace_id,
                    endpoint=lease.endpoint,
                    report_reference=lease.report_reference,
                    reason="lease-expired",
                )
            lease.renew(self._lease_ttl)
            snapshot = _service_snapshot(service)
            return ExternalIedManualReportResult(
                workspace_id=lease.workspace_id,
                endpoint=lease.endpoint,
                report_reference=lease.report_reference,
                enabled=True,
                status=str(snapshot.last_state.runtime_status.value if snapshot and snapshot.last_state else "enabled"),
                **_lease_fields(lease),
            )

    def send_general_interrogation(
        self,
        *,
        workspace_id: int,
        endpoint: str,
        lease_id: str,
    ) -> ExternalIedManualReportResult:
        with self._lock:
            session_key = self._session_by_lease_id.get(lease_id)
            if session_key is None:
                raise ValueError("External IED manual report lease was not found.")
            lease = self._leases_by_session.get(session_key)
            if lease is None or lease.workspace_id != workspace_id or lease.endpoint != endpoint:
                raise ValueError("External IED manual report lease does not match this endpoint.")
            service = self._sessions.get(session_key)
            if service is None or _service_report_enabled(service) is False:
                return self._release_session_locked(
                    session_key,
                    workspace_id=lease.workspace_id,
                    endpoint=lease.endpoint,
                    report_reference=lease.report_reference,
                    reason="report-disabled",
                )
            send_gi = getattr(service, "send_general_interrogation", None)
            if not callable(send_gi):
                raise ValueError("External IED manual report session does not support GI.")
            snapshot = send_gi()
            lease.renew(self._lease_ttl)
            self._remember_report_signature_locked(session_key, snapshot)
            return ExternalIedManualReportResult(
                workspace_id=lease.workspace_id,
                endpoint=lease.endpoint,
                report_reference=lease.report_reference,
                enabled=True,
                status=str(snapshot.last_state.runtime_status.value if snapshot.last_state else "enabled"),
                message=snapshot.last_diagnostic.message if snapshot.last_diagnostic else None,
                signal_states=_snapshot_signal_states(snapshot),
                report_values=_snapshot_report_values(snapshot),
                **_lease_fields(lease),
            )

    def release_lease(
        self,
        *,
        workspace_id: int,
        endpoint: str,
        lease_id: str,
    ) -> ExternalIedManualReportResult:
        with self._lock:
            session_key = self._session_by_lease_id.get(lease_id)
            if session_key is None:
                raise ValueError("External IED manual report lease was not found.")
            lease = self._leases_by_session.get(session_key)
            if lease is None or lease.workspace_id != workspace_id or lease.endpoint != endpoint:
                raise ValueError("External IED manual report lease does not match this endpoint.")
            return self._release_session_locked(
                session_key,
                workspace_id=lease.workspace_id,
                endpoint=lease.endpoint,
                report_reference=lease.report_reference,
                reason="lease-release",
            )

    def cleanup_expired_leases(self) -> None:
        now = _utc_now()
        with self._lock:
            expired = [
                session_key
                for session_key, lease in self._leases_by_session.items()
                if lease.expires_at <= now
            ]
            for session_key in expired:
                lease = self._leases_by_session.get(session_key)
                if lease is None:
                    continue
                try:
                    self._release_session_locked(
                        session_key,
                        workspace_id=lease.workspace_id,
                        endpoint=lease.endpoint,
                        report_reference=lease.report_reference,
                        reason="lease-expired",
                    )
                except Exception:
                    _logger.exception(
                        "Failed to cleanup expired External IED manual report lease",
                        extra={
                            "workspace_id": lease.workspace_id,
                            "endpoint": lease.endpoint,
                            "report_reference": lease.report_reference,
                            "lease_id": lease.lease_id,
                        },
                    )

    def _cleanup_expired_leases_loop(self) -> None:
        while not self._stop_cleanup.wait(self._cleanup_interval_seconds):
            self.cleanup_expired_leases()

    def _poll_manual_report_values_loop(self) -> None:
        while not self._stop_cleanup.wait(self._report_poll_interval_seconds):
            try:
                self.poll_manual_report_values_once()
            except Exception:
                _logger.exception("Failed to poll External IED manual report values")

    def poll_manual_report_values_once(self) -> int:
        with self._lock:
            entries = [
                (session_key, lease, service)
                for session_key, lease in self._leases_by_session.items()
                if lease.expires_at > _utc_now()
                for service in [self._sessions.get(session_key)]
                if service is not None
            ]

        published = 0
        for session_key, lease, service in entries:
            refresh = getattr(service, "refresh_reporting", None)
            if not callable(refresh):
                continue
            try:
                snapshot = refresh()
            except Exception:
                _logger.exception(
                    "Failed to refresh External IED manual report values",
                    extra={
                        "workspace_id": lease.workspace_id,
                        "endpoint": lease.endpoint,
                        "report_reference": lease.report_reference,
                        "lease_id": lease.lease_id,
                    },
                )
                continue

            signal_states = _snapshot_signal_states(snapshot)
            report_values = _snapshot_report_values(snapshot)
            if not signal_states and not report_values:
                continue
            signature = _report_payload_signature(signal_states, report_values)
            with self._lock:
                current_lease = self._leases_by_session.get(session_key)
                if current_lease is None or current_lease.lease_id != lease.lease_id:
                    continue
                if self._last_published_report_signatures.get(session_key) == signature:
                    continue
                self._last_published_report_signatures[session_key] = signature

            event = _manual_report_values_event(
                lease=lease,
                status=str(snapshot.last_state.runtime_status.value if snapshot and snapshot.last_state else "enabled"),
                signal_states=signal_states,
                report_values=report_values,
            )
            try:
                self._event_publisher(event)
                published += 1
            except Exception:
                _logger.exception(
                    "Failed to publish External IED manual report values",
                    extra={
                        "workspace_id": lease.workspace_id,
                        "endpoint": lease.endpoint,
                        "report_reference": lease.report_reference,
                        "lease_id": lease.lease_id,
                    },
                )
        return published

    def _release_session_locked(
        self,
        session_key: str,
        *,
        workspace_id: int,
        endpoint: str,
        report_reference: str,
        reason: str,
    ) -> ExternalIedManualReportResult:
        lease = self._leases_by_session.pop(session_key, None)
        if lease is not None:
            self._session_by_lease_id.pop(lease.lease_id, None)
        self._last_published_report_signatures.pop(session_key, None)
        service = self._sessions.pop(session_key, None)
        if service is not None:
            disconnect = getattr(service, "disconnect_ied", None)
            if callable(disconnect):
                disconnect()
            service.close_ied()
        return ExternalIedManualReportResult(
            workspace_id=workspace_id,
            endpoint=endpoint,
            report_reference=report_reference,
            enabled=False,
            status="disabled",
            message=reason,
        )

    def _remember_report_signature_locked(self, session_key: str, snapshot) -> None:
        signal_states = _snapshot_signal_states(snapshot)
        report_values = _snapshot_report_values(snapshot)
        if signal_states or report_values:
            self._last_published_report_signatures[session_key] = _report_payload_signature(signal_states, report_values)


def _parse_endpoint(endpoint: str) -> Iec61850DeviceEndpoint:
    host, separator, raw_port = endpoint.rpartition(":")
    if separator != ":" or not host.strip():
        raise ValueError("External IED endpoint must be formatted as host:port.")
    try:
        port = int(raw_port)
    except ValueError as exc:
        raise ValueError("External IED endpoint port must be a number.") from exc
    if port < 1 or port > 65535:
        raise ValueError("External IED endpoint port is out of range.")
    return Iec61850DeviceEndpoint(
        id=f"external-ied:{host.strip()}:{port}",
        mode=Iec61850RuntimeMode.MMS,
        ied_name="",
        access_point_name="AP1",
        host=host.strip(),
        port=port,
    )


def _utc_now() -> datetime:
    return datetime.now(UTC)


def _lease_fields(lease: _ExternalIedManualReportLease) -> dict[str, str]:
    return {
        "lease_id": lease.lease_id,
        "owner": lease.owner,
        "created_at": lease.created_at.isoformat(),
        "renewed_at": lease.renewed_at.isoformat(),
        "expires_at": lease.expires_at.isoformat(),
    }


def _service_report_enabled(service: Iec61850ClientControlService) -> bool | None:
    snapshot = _service_snapshot(service)
    if snapshot is None:
        return None
    last_state = getattr(snapshot, "last_state", None)
    if last_state is None:
        return None
    return bool(getattr(last_state, "enabled", False))


def _service_snapshot(service: Iec61850ClientControlService):
    snapshot_method = getattr(service, "snapshot", None)
    if not callable(snapshot_method):
        return None
    return snapshot_method()


def _snapshot_signal_states(snapshot) -> tuple[dict[str, Any], ...]:
    ui_state = getattr(snapshot, "ui_state", None)
    if not isinstance(ui_state, dict):
        return ()
    report = ui_state.get("report")
    if not isinstance(report, dict):
        return ()
    signal_states = report.get("signal_states")
    if not isinstance(signal_states, list):
        return ()
    return tuple(state for state in signal_states if isinstance(state, dict))


def _snapshot_report_values(snapshot) -> tuple[dict[str, Any], ...]:
    ui_state = getattr(snapshot, "ui_state", None)
    if not isinstance(ui_state, dict):
        return ()
    report = ui_state.get("report")
    if not isinstance(report, dict):
        return ()
    values = report.get("values")
    if not isinstance(values, list):
        return ()
    return tuple(value for value in values if isinstance(value, dict))


def _report_payload_signature(signal_states: tuple[dict[str, Any], ...], report_values: tuple[dict[str, Any], ...]) -> str:
    return json.dumps(
        {
            "signal_states": signal_states,
            "report_values": report_values,
        },
        sort_keys=True,
        separators=(",", ":"),
        default=str,
    )


def _endpoint_host_port(endpoint: str) -> tuple[str, int]:
    host, separator, raw_port = endpoint.rpartition(":")
    if separator != ":":
        return endpoint, 102
    try:
        return host, int(raw_port)
    except ValueError:
        return host, 102


def _manual_report_values_event(
    *,
    lease: _ExternalIedManualReportLease,
    status: str,
    signal_states: tuple[dict[str, Any], ...],
    report_values: tuple[dict[str, Any], ...],
) -> dict[str, Any]:
    host, port = _endpoint_host_port(lease.endpoint)
    event = ExternalIedManualReportValuesChangedEvent(
        workspace_id=lease.workspace_id,
        endpoint=lease.endpoint,
        ip=host,
        port=port,
        report_reference=lease.report_reference,
        lease_id=lease.lease_id,
        status=status,
        signal_states=list(signal_states),
        report_values=list(report_values),
        emitted_at=_utc_now().isoformat(),
    )
    return event.model_dump(mode="json")


def _publish_ws_event_sync(event: dict[str, Any]) -> None:
    global _sync_redis
    if _sync_redis is None:
        _sync_redis = Redis.from_url(_settings.redis_url, encoding="utf-8", decode_responses=True)
    _sync_redis.publish(_settings.ws_events_channel, json.dumps(event, separators=(",", ":")))


def _candidate_from_report_request(request: ExternalIedManualReportRequest) -> Iec61850ReportControlCandidate:
    parsed = _parse_report_reference(request.report_reference, request.report_kind)
    report_name = (request.report_name or parsed.report_name).strip()
    dataset_reference = (request.dataset_reference or "").strip() or None
    return Iec61850ReportControlCandidate(
        id=f"{parsed.domain}:{parsed.item}",
        ied_name="",
        access_point_name="AP1",
        logical_device_inst=parsed.domain,
        logical_node_name=parsed.logical_node,
        report_control_name=report_name,
        report_kind=parsed.report_kind,
        rpt_id=None,
        data_set_ref=dataset_reference,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(
            data_change=True,
            quality_change=True,
            data_update=True,
            periodic=False,
            general_interrogation=True,
        ),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference=dataset_reference or parsed.domain, fc=None),),
    )


@dataclass(frozen=True, slots=True)
class _ParsedReportReference:
    domain: str
    item: str
    logical_node: str
    report_name: str
    report_kind: Iec61850ReportKind


def _parse_report_reference(reference: str, report_kind: str | None) -> _ParsedReportReference:
    text = reference.strip()
    if not text:
        raise ValueError("Report reference is required.")
    if ":" in text:
        domain, item = [part.strip() for part in text.split(":", 1)]
    elif " " in text and "$" in text:
        domain, item = [part.strip() for part in text.split(None, 1)]
    else:
        domain, item = _parse_path_report_reference(text, report_kind)
    if not domain or not item:
        raise ValueError("Report reference must include MMS domain and RCB item.")
    if "$BR$" not in item and "$RP$" not in item:
        raise ValueError("Report reference must identify a buffered or unbuffered RCB.")
    parts = item.split("$")
    logical_node = parts[0].strip() if parts else ""
    folder = parts[1].strip() if len(parts) > 1 else ""
    report_name = parts[2].strip() if len(parts) > 2 else ""
    if not logical_node or not report_name:
        raise ValueError("Report reference does not include LN and report control name.")
    kind = Iec61850ReportKind.UNBUFFERED if folder == "RP" else Iec61850ReportKind.BUFFERED
    return _ParsedReportReference(
        domain=domain,
        item=item,
        logical_node=logical_node,
        report_name=report_name,
        report_kind=kind,
    )


def _parse_path_report_reference(reference: str, report_kind: str | None) -> tuple[str, str]:
    value = reference.split("!", 1)[1] if "!" in reference else reference
    domain, separator, rest = value.partition("/")
    if separator != "/" or not domain.strip() or not rest.strip():
        raise ValueError("Report reference must be formatted as LD/LN.Report or LD:LN$BR$Report.")
    normalized = re.sub(r"[/\.]+", "$", rest.strip())
    parts = [part for part in normalized.split("$") if part]
    if len(parts) >= 3 and parts[1] in {"BR", "RP"}:
        return domain.strip(), "$".join(parts[:3])
    logical_node = parts[0] if parts else ""
    report_name = parts[-1] if parts else ""
    folder = "RP" if str(report_kind or "").lower().startswith("unbuffer") else "BR"
    if not logical_node or not report_name:
        raise ValueError("Report reference does not include LN and report control name.")
    return domain.strip(), f"{logical_node}${folder}${report_name}"


def _session_key(workspace_id: int, endpoint: Iec61850DeviceEndpoint, candidate_id: str) -> str:
    return f"{workspace_id}|{endpoint.host}:{endpoint.port}|{candidate_id}"


_manual_report_control_service = ExternalIedManualReportControlService()


def get_external_ied_manual_report_control_service() -> ExternalIedManualReportControlService:
    return _manual_report_control_service
