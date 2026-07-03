from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor, as_completed
from contextlib import suppress
from dataclasses import dataclass, field
from datetime import UTC, datetime
import socket
from threading import RLock, Thread
from typing import Any, Callable, Sequence
from uuid import uuid4

from app.schemas.verification_schema import (
    SignalVerificationEvidenceSetSchema,
    VerificationExecutionContextSchema,
    VerificationEvidenceDiagnosticSchema,
    VerificationRecoveryStateSchema,
    VerificationRunSchema,
    VerificationSessionSnapshotSchema,
    VerificationSubscriptionSnapshotSchema,
    VerificationSubscriptionPlanSchema,
    VerificationTargetSchema,
)
from app.services.iec61850.report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportRuntimeError,
    Iec61850ReportRuntimeAdapter,
    Iec61850ReportRuntimeService,
    Iec61850RuntimeDiagnostic,
    build_simulator_endpoint_for_plan_device,
    create_iec61850_simulator_adapter,
    group_report_subscription_plan_devices_by_endpoint,
    map_report_event_to_signal_observations,
)
from app.services.verification_evidence import build_signal_verification_evidence_set
from app.services.verification_execution import (
    _build_step_and_evidence,
    _parse_timestamp,
    _resolve_runtime_state,
    _unique_non_empty_strings,
    build_runtime_subscription_plan,
)


_MMS_ORCHESTRATION_PREFLIGHT_TIMEOUT_SECONDS = 1.0
_MMS_ORCHESTRATION_PREFLIGHT_MAX_WORKERS = 16
_MMS_ORCHESTRATION_ENDPOINT_MAX_WORKERS = 16


@dataclass
class VerificationRuntimeSessionState:
    session_id: str
    endpoint_id: str
    runtime_state: str = "connecting"
    connection_generation: int = 1
    discovery_status: str = "discovering"
    last_error: str | None = None
    diagnostic_code: str | None = None

    def to_snapshot(self) -> VerificationSessionSnapshotSchema:
        return VerificationSessionSnapshotSchema(
            session_id=self.session_id,
            endpoint_id=self.endpoint_id,
            runtime_state=self.runtime_state,
            connection_generation=self.connection_generation,
            discovery_status=self.discovery_status,
            last_error=self.last_error,
            diagnostic_code=self.diagnostic_code,
        )


@dataclass
class VerificationRuntimeSubscriptionState:
    subscription_id: str
    session_id: str
    endpoint_id: str
    group_id: str | None = None
    report_control_reference: str | None = None
    report_control_name: str | None = None
    data_set_reference: str | None = None
    subscription_state: str = "pending"
    report_health: str = "unknown"
    last_report_at: datetime | None = None
    gi_requested: bool = False
    last_report_value_count: int = 0
    last_report_values: list[dict[str, Any]] = field(default_factory=list)
    last_sequence_number: int | None = None
    last_report_id: str | None = None
    current_rptena_owner: str | None = None
    stale_signal_count: int | None = None
    last_error: str | None = None
    diagnostic_code: str | None = None
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = field(default_factory=list)

    def to_snapshot(self) -> VerificationSubscriptionSnapshotSchema:
        return VerificationSubscriptionSnapshotSchema(
            subscription_id=self.subscription_id,
            session_id=self.session_id,
            endpoint_id=self.endpoint_id,
            group_id=self.group_id,
            report_control_reference=self.report_control_reference,
            report_control_name=self.report_control_name,
            data_set_reference=self.data_set_reference,
            subscription_state=self.subscription_state,
            report_health=self.report_health,
            last_report_at=self.last_report_at,
            gi_requested=self.gi_requested,
            last_report_value_count=self.last_report_value_count,
            last_report_values=list(self.last_report_values),
            current_rptena_owner=self.current_rptena_owner,
            stale_signal_count=self.stale_signal_count,
            last_error=self.last_error,
            diagnostic_code=self.diagnostic_code,
            diagnostics=list(self.diagnostics),
        )


@dataclass(frozen=True, slots=True)
class VerificationRuntimeOrchestrationResult:
    orchestration_id: str
    verification_run: VerificationRunSchema
    session_snapshots: tuple[VerificationSessionSnapshotSchema, ...]
    subscription_snapshots: tuple[VerificationSubscriptionSnapshotSchema, ...]
    diagnostics: tuple[VerificationEvidenceDiagnosticSchema, ...]
    active_session_ids: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class VerificationRuntimeSignalCaptureResult:
    evidence: Any
    step: Any
    diagnostics: tuple[VerificationEvidenceDiagnosticSchema, ...]


@dataclass(frozen=True, slots=True)
class _CapturedRuntimeResult:
    diagnostics: tuple[Any, ...] = ()


@dataclass(frozen=True, slots=True)
class _CapturedRuntimeReport:
    candidate_id: str
    endpoint_id: str | None
    ied_name: str
    access_point_name: str
    report_control_name: str
    data_set_ref: str | None
    event: Any
    diagnostics: tuple[Any, ...]
    error_code: str | None
    error_message: str | None


@dataclass
class _VerificationRuntimeOrchestrationHandle:
    workspace_id: int
    test_run_id: str
    runtime_service: Iec61850ReportRuntimeService
    session_states: dict[str, VerificationRuntimeSessionState]
    subscription_states: dict[str, VerificationRuntimeSubscriptionState]
    session_order: list[str]
    subscription_order: list[str]
    verification_targets: list[VerificationTargetSchema]
    subscription_plan: VerificationSubscriptionPlanSchema
    execution_context: VerificationExecutionContextSchema
    client_id: str
    started_at: datetime
    diagnostics: list[VerificationEvidenceDiagnosticSchema]
    reconnecting_session_ids: set[str] = field(default_factory=set)
    endpoint_for_device: Callable[[Any], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device
    lock: RLock = field(default_factory=RLock)


class VerificationRuntimeOrchestrator:
    def __init__(
        self,
        *,
        now: Callable[[], datetime] | None = None,
        mms_reachability_probe: Callable[[Iec61850DeviceEndpoint], tuple[bool, str | None]] | None = None,
    ) -> None:
        self._now = now or (lambda: datetime.now(UTC))
        self._mms_reachability_probe = mms_reachability_probe or _probe_mms_endpoint_reachability
        self._handles: dict[str, _VerificationRuntimeOrchestrationHandle] = {}

    def start(
        self,
        *,
        workspace_id: int,
        test_run_id: str,
        verification_targets: Sequence[VerificationTargetSchema],
        subscription_plan: VerificationSubscriptionPlanSchema,
        execution_context: VerificationExecutionContextSchema,
        client_id: str = "unitlab-backend-simulator",
        endpoint_for_device: Callable[[Any], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
        adapter: Iec61850ReportRuntimeAdapter | None = None,
        initial_diagnostics: Sequence[VerificationEvidenceDiagnosticSchema] = (),
    ) -> VerificationRuntimeOrchestrationResult:
        orchestration_id = f"{workspace_id}:{test_run_id}:{uuid4().hex[:8]}"
        if orchestration_id in self._handles:
            raise RuntimeError(f'Verification orchestration "{orchestration_id}" already exists.')
        started_at = self._now()

        runtime_plan = build_runtime_subscription_plan(subscription_plan)
        runtime_adapter = adapter or create_iec61850_simulator_adapter(now=self._now)
        runtime_service = Iec61850ReportRuntimeService(runtime_adapter)
        session_states: dict[str, VerificationRuntimeSessionState] = {}
        subscription_states: dict[str, VerificationRuntimeSubscriptionState] = {}
        session_order: list[str] = []
        subscription_order: list[str] = []
        diagnostics: list[VerificationEvidenceDiagnosticSchema] = list(initial_diagnostics)

        device_groups = group_report_subscription_plan_devices_by_endpoint(
            plan=runtime_plan,
            endpoint_for_device=endpoint_for_device,
        )
        unreachable_endpoints = self._preflight_mms_endpoints(device_groups)

        try:
            for group_index, device_group in enumerate(device_groups):
                endpoint = device_group.endpoint
                session_id = f"{orchestration_id}:{group_index}:{endpoint.ied_name}/{endpoint.access_point_name}"
                group_reports = tuple(report for device in device_group.devices for report in device.reports)
                session_order.append(session_id)
                session_states[session_id] = VerificationRuntimeSessionState(
                    session_id=session_id,
                    endpoint_id=endpoint.id,
                )
                unreachable_error = unreachable_endpoints.get(endpoint.id)
                if unreachable_error is not None:
                    session_states[session_id].runtime_state = "failed"
                    session_states[session_id].discovery_status = "unavailable"
                    session_states[session_id].last_error = unreachable_error.message
                    session_states[session_id].diagnostic_code = unreachable_error.code
                    for report in group_reports:
                        subscription_state = self._activate_report_subscription(
                            runtime_service=runtime_service,
                            session_state=session_states[session_id],
                            endpoint=endpoint,
                            report=report,
                            client_id=client_id,
                            diagnostics=diagnostics,
                            session_id=session_id,
                        )
                        subscription_states[subscription_state.subscription_id] = subscription_state
                        subscription_order.append(subscription_state.subscription_id)
                    continue

                runtime_service.open_session(
                    session_id=session_id,
                    endpoint=endpoint,
                    candidates=[report.candidate for report in group_reports],
                )
                self._transition(session_states[session_id], "discovering", "discovering")

                for report in group_reports:
                    subscription_state = self._activate_report_subscription(
                        runtime_service=runtime_service,
                        session_state=session_states[session_id],
                        endpoint=endpoint,
                        report=report,
                        client_id=client_id,
                        diagnostics=diagnostics,
                        session_id=session_id,
                    )
                    subscription_states[subscription_state.subscription_id] = subscription_state
                    subscription_order.append(subscription_state.subscription_id)
        except Exception:
            for session_id in reversed(session_order):
                with suppress(Exception):
                    runtime_service.close_session(session_id)
            raise

        handle = _VerificationRuntimeOrchestrationHandle(
            workspace_id=workspace_id,
            test_run_id=test_run_id,
            runtime_service=runtime_service,
            session_states=session_states,
            subscription_states=subscription_states,
            session_order=session_order,
            subscription_order=subscription_order,
            verification_targets=list(verification_targets),
            subscription_plan=subscription_plan,
            execution_context=execution_context,
            client_id=client_id,
            started_at=started_at,
            diagnostics=diagnostics,
            endpoint_for_device=endpoint_for_device,
        )
        self._handles[orchestration_id] = handle
        return self.snapshot(orchestration_id)

    def start_deferred(
        self,
        *,
        workspace_id: int,
        test_run_id: str,
        verification_targets: Sequence[VerificationTargetSchema],
        subscription_plan: VerificationSubscriptionPlanSchema,
        execution_context: VerificationExecutionContextSchema,
        client_id: str = "unitlab-backend-simulator",
        endpoint_for_device: Callable[[Any], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
        adapter: Iec61850ReportRuntimeAdapter | None = None,
        initial_diagnostics: Sequence[VerificationEvidenceDiagnosticSchema] = (),
    ) -> VerificationRuntimeOrchestrationResult:
        orchestration_id = f"{workspace_id}:{test_run_id}:{uuid4().hex[:8]}"
        if orchestration_id in self._handles:
            raise RuntimeError(f'Verification orchestration "{orchestration_id}" already exists.')

        runtime_plan = build_runtime_subscription_plan(subscription_plan)
        runtime_adapter = adapter or create_iec61850_simulator_adapter(now=self._now)
        runtime_service = Iec61850ReportRuntimeService(runtime_adapter)
        session_states: dict[str, VerificationRuntimeSessionState] = {}
        subscription_states: dict[str, VerificationRuntimeSubscriptionState] = {}
        session_order: list[str] = []
        subscription_order: list[str] = []
        diagnostics: list[VerificationEvidenceDiagnosticSchema] = list(initial_diagnostics)

        device_groups = group_report_subscription_plan_devices_by_endpoint(
            plan=runtime_plan,
            endpoint_for_device=endpoint_for_device,
        )
        for group_index, device_group in enumerate(device_groups):
            endpoint = device_group.endpoint
            session_id = f"{orchestration_id}:{group_index}:{endpoint.ied_name}/{endpoint.access_point_name}"
            session_order.append(session_id)
            session_states[session_id] = VerificationRuntimeSessionState(
                session_id=session_id,
                endpoint_id=endpoint.id,
                runtime_state="connecting",
                discovery_status="discovering",
            )
            for report in tuple(report for device in device_group.devices for report in device.reports):
                subscription_state = self._pending_report_subscription(
                    session_id=session_id,
                    endpoint=endpoint,
                    report=report,
                )
                subscription_states[subscription_state.subscription_id] = subscription_state
                subscription_order.append(subscription_state.subscription_id)

        handle = _VerificationRuntimeOrchestrationHandle(
            workspace_id=workspace_id,
            test_run_id=test_run_id,
            runtime_service=runtime_service,
            session_states=session_states,
            subscription_states=subscription_states,
            session_order=session_order,
            subscription_order=subscription_order,
            verification_targets=list(verification_targets),
            subscription_plan=subscription_plan,
            execution_context=execution_context,
            client_id=client_id,
            started_at=self._now(),
            diagnostics=diagnostics,
            endpoint_for_device=endpoint_for_device,
        )
        self._handles[orchestration_id] = handle
        Thread(
            target=self._run_deferred_startup,
            args=(orchestration_id,),
            name=f"iec61850-online-{orchestration_id}",
            daemon=True,
        ).start()
        return self.snapshot(orchestration_id)

    def _run_deferred_startup(self, orchestration_id: str) -> None:
        handle = self._handles.get(orchestration_id)
        if handle is None:
            return
        runtime_plan = build_runtime_subscription_plan(handle.subscription_plan)
        device_groups = group_report_subscription_plan_devices_by_endpoint(
            plan=runtime_plan,
            endpoint_for_device=handle.endpoint_for_device,
        )
        unreachable_endpoints = self._preflight_mms_endpoints(device_groups)

        worker_count = min(len(device_groups), _MMS_ORCHESTRATION_ENDPOINT_MAX_WORKERS)
        if worker_count <= 0:
            return
        with ThreadPoolExecutor(max_workers=worker_count, thread_name_prefix="iec61850-online-endpoint") as executor:
            futures = [
                executor.submit(
                    self._run_deferred_endpoint_group,
                    handle,
                    group_index,
                    device_group,
                    unreachable_endpoints.get(device_group.endpoint.id),
                )
                for group_index, device_group in enumerate(device_groups)
            ]
            for future in as_completed(futures):
                future.result()

    def _run_deferred_endpoint_group(
        self,
        handle: _VerificationRuntimeOrchestrationHandle,
        group_index: int,
        device_group: Any,
        unreachable_error: Iec61850ReportRuntimeError | None,
    ) -> None:
        try:
            self._run_deferred_endpoint_group_inner(handle, group_index, device_group, unreachable_error)
        except Exception as exc:  # noqa: BLE001
            self._fail_deferred_endpoint_group(handle, group_index, device_group, exc)

    def _run_deferred_endpoint_group_inner(
        self,
        handle: _VerificationRuntimeOrchestrationHandle,
        group_index: int,
        device_group: Any,
        unreachable_error: Iec61850ReportRuntimeError | None,
    ) -> None:
        endpoint = device_group.endpoint
        if group_index >= len(handle.session_order):
            return
        session_id = handle.session_order[group_index]
        session_state = handle.session_states[session_id]
        group_reports = tuple(report for device in device_group.devices for report in device.reports)
        if unreachable_error is not None:
            with handle.lock:
                self._mark_session_failed(session_state, unreachable_error, discovery_available=False)
            for report in group_reports:
                self._store_subscription_state(
                    handle,
                    self._activate_report_subscription(
                        runtime_service=handle.runtime_service,
                        session_state=session_state,
                        endpoint=endpoint,
                        report=report,
                        client_id=handle.client_id,
                        diagnostics=handle.diagnostics,
                        session_id=session_id,
                    ),
                )
            return

        try:
            handle.runtime_service.open_session(
                session_id=session_id,
                endpoint=endpoint,
                candidates=[report.candidate for report in group_reports],
            )
        except Iec61850ReportRuntimeError as exc:
            with handle.lock:
                self._mark_session_failed(session_state, exc, discovery_available=False)
            for report in group_reports:
                self._store_subscription_state(
                    handle,
                    self._activate_report_subscription(
                        runtime_service=handle.runtime_service,
                        session_state=session_state,
                        endpoint=endpoint,
                        report=report,
                        client_id=handle.client_id,
                        diagnostics=handle.diagnostics,
                        session_id=session_id,
                    ),
                )
            return

        with handle.lock:
            self._transition(session_state, "discovering", "discovering")
        for report in group_reports:
            if session_state.runtime_state == "closed":
                break
            self._store_subscription_state(
                handle,
                self._activate_report_subscription(
                    runtime_service=handle.runtime_service,
                    session_state=session_state,
                    endpoint=endpoint,
                    report=report,
                    client_id=handle.client_id,
                    diagnostics=handle.diagnostics,
                    session_id=session_id,
                ),
            )

    def _fail_deferred_endpoint_group(
        self,
        handle: _VerificationRuntimeOrchestrationHandle,
        group_index: int,
        device_group: Any,
        exc: Exception,
    ) -> None:
        if group_index >= len(handle.session_order):
            return
        endpoint = device_group.endpoint
        session_id = handle.session_order[group_index]
        session_state = handle.session_states[session_id]
        error = (
            exc
            if isinstance(exc, Iec61850ReportRuntimeError)
            else Iec61850ReportRuntimeError(
                "MMS_ORCHESTRATION_ENDPOINT_FAILED",
                f"IEC 61850 endpoint orchestration failed: {exc}",
            )
        )
        group_reports = tuple(report for device in device_group.devices for report in device.reports)
        with handle.lock:
            self._mark_session_failed(session_state, error, discovery_available=session_state.discovery_status == "available")
        for report in group_reports:
            subscription_id = _resolve_subscription_id_for_runtime(session_id=session_id, report=report)
            with handle.lock:
                current = handle.subscription_states.get(subscription_id)
                should_replace = current is None or current.subscription_state == "pending"
            if not should_replace:
                continue
            self._store_subscription_state(
                handle,
                self._activate_report_subscription(
                    runtime_service=handle.runtime_service,
                    session_state=session_state,
                    endpoint=endpoint,
                    report=report,
                    client_id=handle.client_id,
                    diagnostics=handle.diagnostics,
                    session_id=session_id,
                ),
            )

    def _pending_report_subscription(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        report,
    ) -> VerificationRuntimeSubscriptionState:
        return VerificationRuntimeSubscriptionState(
            subscription_id=_resolve_subscription_id_for_runtime(session_id=session_id, report=report),
            session_id=session_id,
            endpoint_id=endpoint.id,
            group_id=report.candidate.id,
            report_control_reference=_candidate_report_reference(report.candidate),
            report_control_name=report.candidate.report_control_name or report.candidate.logical_node_name or report.candidate.id,
            data_set_reference=report.candidate.data_set_ref or _candidate_signal_scope(report.candidate),
            subscription_state="pending",
            report_health="unknown",
        )

    def _store_subscription_state(
        self,
        handle: _VerificationRuntimeOrchestrationHandle,
        subscription_state: VerificationRuntimeSubscriptionState,
    ) -> None:
        with handle.lock:
            handle.subscription_states[subscription_state.subscription_id] = subscription_state
            if subscription_state.subscription_id not in handle.subscription_order:
                handle.subscription_order.append(subscription_state.subscription_id)

    def _mark_session_failed(
        self,
        session_state: VerificationRuntimeSessionState,
        error: Iec61850ReportRuntimeError,
        *,
        discovery_available: bool,
    ) -> None:
        session_state.runtime_state = "failed"
        session_state.discovery_status = "available" if discovery_available else "unavailable"
        session_state.last_error = error.message
        session_state.diagnostic_code = error.code

    def _preflight_mms_endpoints(
        self,
        device_groups: Sequence[Any],
    ) -> dict[str, Iec61850ReportRuntimeError]:
        endpoints = [
            group.endpoint
            for group in device_groups
            if group.endpoint.mode.name == "MMS" and group.endpoint.host is not None and str(group.endpoint.host).strip()
        ]
        if not endpoints:
            return {}

        failures: dict[str, Iec61850ReportRuntimeError] = {}
        worker_count = min(len(endpoints), _MMS_ORCHESTRATION_PREFLIGHT_MAX_WORKERS)
        with ThreadPoolExecutor(max_workers=worker_count, thread_name_prefix="iec61850-mms-preflight") as executor:
            futures = {executor.submit(self._mms_reachability_probe, endpoint): endpoint for endpoint in endpoints}
            for future in as_completed(futures):
                endpoint = futures[future]
                try:
                    reachable, detail = future.result()
                except Exception as exc:  # noqa: BLE001
                    reachable = False
                    detail = str(exc)
                if reachable:
                    continue
                host = str(endpoint.host or "").strip()
                port = int(endpoint.port)
                suffix = f": {detail}" if detail else ""
                failures[endpoint.id] = Iec61850ReportRuntimeError(
                    "EXTERNAL_MMS_ENDPOINT_UNREACHABLE",
                    f"IEC 61850 endpoint {host}:{port} is unreachable{suffix}",
                )
        return failures

    def snapshot(self, orchestration_id: str) -> VerificationRuntimeOrchestrationResult:
        handle = self._require_handle(orchestration_id)
        with handle.lock:
            session_snapshots = tuple(handle.session_states[session_id].to_snapshot() for session_id in handle.session_order)
            subscription_snapshots = tuple(
                handle.subscription_states[subscription_id].to_snapshot() for subscription_id in handle.subscription_order
            )
            diagnostics = list(handle.diagnostics)
            verification_targets = list(handle.verification_targets)
            subscription_plan = handle.subscription_plan
            execution_context = handle.execution_context
            test_run_id = handle.test_run_id
            started_at = handle.started_at
            client_id = handle.client_id
        evidence_set = build_signal_verification_evidence_set(
            test_run_id=test_run_id,
            evidence=(),
            diagnostics=diagnostics,
        )
        workflow_state = "running"
        verdict_state = "pending"
        runtime_state = _resolve_runtime_state(session_snapshots, subscription_snapshots) or "connecting"
        verification_run = VerificationRunSchema(
            test_run_id=test_run_id,
            verification_targets=verification_targets,
            subscription_plan=subscription_plan,
            session_snapshots=list(session_snapshots),
            subscription_snapshots=list(subscription_snapshots),
            evidence_set=evidence_set,
            execution_context=execution_context,
            recovery_state=_build_recovery_state_for_orchestration(
                test_run_id=test_run_id,
                verification_targets=verification_targets,
                subscription_plan=subscription_plan,
                execution_context=execution_context,
                session_snapshots=session_snapshots,
                subscription_snapshots=subscription_snapshots,
            ),
            workflow_state=workflow_state,
            verdict_state=verdict_state,
            triggered_at=started_at,
            completed_at=None,
            runtime_state=runtime_state,
            runtime_summary=_build_runtime_summary_for_orchestration(
                session_snapshots=session_snapshots,
                subscription_snapshots=subscription_snapshots,
                client_id=client_id,
                diagnostics=diagnostics,
            ),
            diagnostics=diagnostics,
            verification_steps=[],
        )
        return VerificationRuntimeOrchestrationResult(
            orchestration_id=orchestration_id,
            verification_run=verification_run,
            session_snapshots=session_snapshots,
            subscription_snapshots=subscription_snapshots,
            diagnostics=tuple(diagnostics),
            active_session_ids=tuple(handle.session_order),
        )

    def stop(self, orchestration_id: str) -> VerificationRuntimeOrchestrationResult:
        handle = self._require_handle(orchestration_id)
        for session_id in reversed(handle.session_order):
            with suppress(Exception):
                handle.runtime_service.close_session(session_id)
            if session_id in handle.session_states:
                handle.session_states[session_id].runtime_state = "closed"
        for subscription_id in handle.subscription_order:
            if subscription_id in handle.subscription_states:
                handle.subscription_states[subscription_id].subscription_state = "closed"
                handle.subscription_states[subscription_id].report_health = "unknown"
        result = self.snapshot(orchestration_id)
        self._handles.pop(orchestration_id, None)
        return result

    def capture_triggered_signal(
        self,
        orchestration_id: str,
        *,
        signal_id: int,
        triggered_at: datetime,
        test_run_id: str | None = None,
        timeout_ms: int | None = None,
    ) -> VerificationRuntimeSignalCaptureResult:
        handle = self._require_handle(orchestration_id)
        target_index, target = self._resolve_target_for_signal(handle, signal_id)
        group = self._resolve_subscription_plan_group_for_target(handle, target_index)
        diagnostics: list[VerificationEvidenceDiagnosticSchema] = []
        observation_bundle = None
        source_generation = None

        if group is None:
            diagnostics.append(
                VerificationEvidenceDiagnosticSchema(
                    code="verification_group_not_found",
                    message="No IEC 61850 subscription group covers the triggered signal.",
                    severity="error",
                    details={"signal_id": int(signal_id)},
                )
            )
        else:
            try:
                runtime_report, session_state, subscription_state = self._resolve_runtime_report_for_group(
                    handle,
                    group.group_id,
                )
                source_generation = session_state.connection_generation
                event = handle.runtime_service.wait_for_report(
                    session_id=session_state.session_id,
                    candidate=runtime_report.candidate,
                    client_id=handle.client_id,
                    after_sequence_number=subscription_state.last_sequence_number,
                    after_event_id=subscription_state.last_report_id,
                    timeout_ms=timeout_ms or int(target.timeout_ms),
                )
                observation_result = map_report_event_to_signal_observations(
                    candidate=runtime_report.candidate,
                    matched_signals=runtime_report.matched_signals,
                    event=event,
                )
                diagnostics.extend(_observation_diagnostics_to_evidence_diagnostics(observation_result.diagnostics))
                captured_report = _CapturedRuntimeReport(
                    candidate_id=runtime_report.candidate.id,
                    endpoint_id=event.endpoint_id,
                    ied_name=runtime_report.candidate.ied_name,
                    access_point_name=runtime_report.candidate.access_point_name,
                    report_control_name=runtime_report.candidate.report_control_name,
                    data_set_ref=runtime_report.candidate.data_set_ref,
                    event=event,
                    diagnostics=tuple(observation_result.diagnostics),
                    error_code=None,
                    error_message=None,
                )
                observation = next(
                    (
                        item
                        for item in observation_result.observations
                        if str(item.selected_signal_id) == str(int(signal_id))
                    ),
                    None,
                )
                if observation is not None:
                    observation_bundle = (
                        captured_report,
                        observation.model_reference,
                        observation.value,
                        observation.timestamp,
                    )
                subscription_state.last_report_at = _parse_timestamp(event.received_at)
                subscription_state.gi_requested = True
                subscription_state.last_report_value_count = len(event.values)
                subscription_state.last_report_values = _report_event_values_payload(event)
                subscription_state.last_sequence_number = event.sequence_number
                subscription_state.last_report_id = event.id
                subscription_state.stale_signal_count = 0 if observation is not None else 1
                subscription_state.report_health = "healthy" if observation is not None else "degraded"
                subscription_state.diagnostic_code = None if observation is not None else "SIGNAL_NOT_INCLUDED_IN_REPORT_EVENT"
            except Iec61850ReportRuntimeError as exc:
                diagnostics.append(
                    VerificationEvidenceDiagnosticSchema(
                        code=exc.code,
                        message=exc.message,
                        severity="error",
                        details={"signal_id": int(signal_id), "group_id": group.group_id},
                    )
                )
                self._mark_group_capture_failed(handle, group.group_id, exc.code, exc.message)

        evidence, step = _build_step_and_evidence(
            target_index=target_index,
            target=target,
            group=group,
            observation_bundle=observation_bundle,
            triggered_at=triggered_at,
            runtime_result=_CapturedRuntimeResult(diagnostics=tuple(diagnostics)),
            test_run_id=test_run_id or handle.test_run_id,
            source_generation=source_generation,
        )
        if diagnostics:
            evidence = evidence.model_copy(update={"diagnostics": [*evidence.diagnostics, *diagnostics]})
            step = step.model_copy(update={"diagnostics": [*step.diagnostics, *diagnostics]})
        return VerificationRuntimeSignalCaptureResult(
            evidence=evidence,
            step=step,
            diagnostics=tuple(diagnostics),
        )

    def reconnect(
        self,
        orchestration_id: str,
        session_id: str,
        *,
        workspace_id: int | None = None,
    ) -> VerificationRuntimeOrchestrationResult:
        handle = self._require_handle(orchestration_id)
        if workspace_id is not None and handle.workspace_id != workspace_id:
            raise RuntimeError(f'Verification orchestration "{orchestration_id}" not found.')
        session_state = handle.session_states.get(session_id)
        if session_state is None:
            raise RuntimeError(f'Verification session "{session_id}" not found.')
        if session_id in handle.reconnecting_session_ids:
            return self.snapshot(orchestration_id)

        handle.reconnecting_session_ids.add(session_id)
        try:
            runtime_plan = build_runtime_subscription_plan(handle.subscription_plan)
            device_group = self._resolve_runtime_plan_device_group(
                runtime_plan,
                session_state.endpoint_id,
                endpoint_for_device=handle.endpoint_for_device,
            )
            endpoint = device_group.endpoint
            group_reports = tuple(report for device in device_group.devices for report in device.reports)
            candidates = [report.candidate for report in group_reports]
            session_subscription_ids = [
                subscription_id
                for subscription_id in handle.subscription_order
                if handle.subscription_states[subscription_id].session_id == session_id
            ]

            session_state.runtime_state = "reconnecting"
            session_state.discovery_status = "discovering"
            session_state.last_error = None
            session_state.diagnostic_code = "USER_RECONNECT"
            for subscription_id in session_subscription_ids:
                subscription_state = handle.subscription_states[subscription_id]
                subscription_state.subscription_state = "reconnecting"
                subscription_state.report_health = "degraded"
                subscription_state.last_error = None
                subscription_state.diagnostic_code = "USER_RECONNECT"

            with suppress(Exception):
                handle.runtime_service.close_session(session_id)

            handle.runtime_service.open_session(
                session_id=session_id,
                endpoint=endpoint,
                candidates=candidates,
            )
            session_state.connection_generation += 1
            self._transition(session_state, "discovering", "discovering")

            for report in group_reports:
                subscription_state = self._activate_report_subscription(
                    runtime_service=handle.runtime_service,
                    session_state=session_state,
                    endpoint=endpoint,
                    report=report,
                    client_id=handle.client_id,
                    diagnostics=handle.diagnostics,
                    session_id=session_id,
                )
                handle.subscription_states[subscription_state.subscription_id] = subscription_state
                if subscription_state.subscription_id not in handle.subscription_order:
                    handle.subscription_order.append(subscription_state.subscription_id)
            session_state.last_error = None
            session_state.diagnostic_code = None
            return self.snapshot(orchestration_id)
        except Exception as exc:  # noqa: BLE001
            session_state.runtime_state = "failed"
            session_state.discovery_status = "available"
            session_state.last_error = str(exc)
            session_state.diagnostic_code = "RECONNECT_FAILED"
            for subscription_id in handle.subscription_order:
                subscription_state = handle.subscription_states[subscription_id]
                if subscription_state.session_id != session_id:
                    continue
                subscription_state.subscription_state = "failed"
                subscription_state.report_health = "degraded"
                subscription_state.last_error = str(exc)
                subscription_state.diagnostic_code = "RECONNECT_FAILED"
            return self.snapshot(orchestration_id)
        finally:
            handle.reconnecting_session_ids.discard(session_id)

    def _activate_report_subscription(
        self,
        *,
        runtime_service: Iec61850ReportRuntimeService,
        session_state: VerificationRuntimeSessionState,
        endpoint: Iec61850DeviceEndpoint,
        report,
        client_id: str,
        diagnostics: list[VerificationEvidenceDiagnosticSchema],
        session_id: str,
    ) -> VerificationRuntimeSubscriptionState:
        subscription_state = VerificationRuntimeSubscriptionState(
            subscription_id=_resolve_subscription_id_for_runtime(session_id=session_id, report=report),
            session_id=session_id,
            endpoint_id=endpoint.id,
            group_id=report.candidate.id,
            report_control_reference=_candidate_report_reference(report.candidate),
            report_control_name=report.candidate.report_control_name or report.candidate.logical_node_name or report.candidate.id,
            data_set_reference=report.candidate.data_set_ref or _candidate_signal_scope(report.candidate),
        )
        if session_state.runtime_state == "failed":
            diagnostic = _session_failure_to_evidence_diagnostic(
                session_state=session_state,
                endpoint=endpoint,
                report=report,
            )
            diagnostics.append(diagnostic)
            subscription_state.diagnostics.append(diagnostic)
            subscription_state.subscription_state = "failed"
            subscription_state.report_health = "degraded"
            subscription_state.last_error = session_state.last_error
            subscription_state.diagnostic_code = session_state.diagnostic_code
            return subscription_state
        discovery_available = False
        try:
            state = runtime_service.read_report_control(
                session_id=session_id,
                endpoint=endpoint,
                candidate=report.candidate,
            )
            subscription_state.report_control_reference = state.state.rpt_id or state.state.reference.report_control_name
            subscription_state.report_control_name = state.state.reference.report_control_name
            subscription_state.data_set_reference = state.state.data_set_ref
            discovery_available = True
            session_diagnostics = _runtime_diagnostics_to_evidence_diagnostics(state.diagnostics)
            diagnostics.extend(session_diagnostics)
            subscription_state.diagnostics.extend(session_diagnostics)
        except Iec61850ReportRuntimeError as exc:
            diagnostic = _runtime_error_to_evidence_diagnostic(
                error=exc,
                endpoint=endpoint,
                report=report,
                session_id=session_id,
            )
            diagnostics.append(diagnostic)
            subscription_state.diagnostics.append(diagnostic)
            subscription_state.subscription_state = "failed"
            subscription_state.report_health = "degraded"
            subscription_state.last_error = exc.message
            subscription_state.diagnostic_code = exc.code
            session_state.runtime_state = "failed"
            session_state.discovery_status = "available" if discovery_available else "unavailable"
            session_state.last_error = exc.message
            session_state.diagnostic_code = exc.code
            return subscription_state
        if any(diagnostic.severity == "error" for diagnostic in session_diagnostics):
            subscription_state.subscription_state = "degraded"
            subscription_state.report_health = "degraded"
            subscription_state.last_error = "ReportControl precheck failed"
            error_diagnostic = next((diagnostic for diagnostic in state.diagnostics if diagnostic.severity == "error"), None)
            subscription_state.diagnostic_code = error_diagnostic.code if error_diagnostic is not None else "REPORT_CONTROL_PRECHECK_FAILED"
            session_state.discovery_status = "available"
            session_state.diagnostic_code = None
            return subscription_state

        session_state.discovery_status = "available"
        try:
            runtime_service.reserve_report_control(session_id=session_id, candidate=report.candidate, client_id=client_id)
            subscription_state.subscription_state = "reserving"
            runtime_service.enable_report_control(session_id=session_id, candidate=report.candidate, client_id=client_id)
            subscription_state.subscription_state = "enabled"
            if _should_defer_startup_general_interrogation(endpoint):
                session_state.runtime_state = "reporting"
                subscription_state.subscription_state = "reporting"
                subscription_state.report_health = "healthy"
                subscription_state.current_rptena_owner = client_id
                subscription_state.stale_signal_count = 0
                session_state.diagnostic_code = None
                return subscription_state
            event = runtime_service.send_general_interrogation(session_id=session_id, candidate=report.candidate, client_id=client_id)
        except Iec61850ReportRuntimeError as exc:
            if exc.code == "MMS_REPORT_NOT_OBSERVED" and endpoint.mode.name == "MMS":
                diagnostic = _runtime_error_to_evidence_diagnostic(
                    error=exc,
                    endpoint=endpoint,
                    report=report,
                    session_id=session_id,
                    severity="warning",
                )
                diagnostics.append(diagnostic)
                subscription_state.diagnostics.append(diagnostic)
                session_state.runtime_state = "reporting"
                session_state.last_error = None
                session_state.diagnostic_code = None
                subscription_state.subscription_state = "reporting"
                subscription_state.report_health = "healthy"
                subscription_state.current_rptena_owner = client_id
                subscription_state.gi_requested = True
                subscription_state.stale_signal_count = 0
                return subscription_state
            diagnostic = _runtime_error_to_evidence_diagnostic(
                error=exc,
                endpoint=endpoint,
                report=report,
                session_id=session_id,
            )
            diagnostics.append(diagnostic)
            subscription_state.diagnostics.append(diagnostic)
            subscription_state.subscription_state = "failed"
            subscription_state.report_health = "degraded"
            subscription_state.last_error = exc.message
            subscription_state.diagnostic_code = exc.code
            session_state.runtime_state = "degraded"
            session_state.last_error = exc.message
            session_state.diagnostic_code = exc.code
            return subscription_state
        session_state.runtime_state = "reporting"
        subscription_state.subscription_state = "reporting"
        subscription_state.report_health = "healthy"
        subscription_state.current_rptena_owner = client_id
        subscription_state.last_report_at = _parse_timestamp(event.received_at)
        subscription_state.gi_requested = True
        subscription_state.last_report_value_count = len(event.values)
        subscription_state.last_report_values = _report_event_values_payload(event)
        subscription_state.last_sequence_number = event.sequence_number
        subscription_state.last_report_id = event.id
        subscription_state.stale_signal_count = 0
        session_state.diagnostic_code = None
        return subscription_state

    def _transition(self, session_state: VerificationRuntimeSessionState, runtime_state: str, discovery_status: str) -> None:
        session_state.runtime_state = runtime_state
        session_state.discovery_status = discovery_status

    def _require_handle(self, orchestration_id: str) -> _VerificationRuntimeOrchestrationHandle:
        handle = self._handles.get(orchestration_id)
        if handle is None:
            raise RuntimeError(f'Verification orchestration "{orchestration_id}" not found.')
        return handle

    def _resolve_runtime_plan_device_group(
        self,
        runtime_plan,
        endpoint_id: str,
        *,
        endpoint_for_device: Callable[[Any], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
    ):
        for device_group in group_report_subscription_plan_devices_by_endpoint(
            plan=runtime_plan,
            endpoint_for_device=endpoint_for_device,
        ):
            if device_group.endpoint.id == endpoint_id:
                return device_group
        raise RuntimeError(f'Runtime device group for endpoint "{endpoint_id}" not found.')

    def _resolve_target_for_signal(
        self,
        handle: _VerificationRuntimeOrchestrationHandle,
        signal_id: int,
    ) -> tuple[int, VerificationTargetSchema]:
        for index, target in enumerate(handle.verification_targets):
            if int(target.signal_id) == int(signal_id):
                return index, target
        raise RuntimeError(f'Verification target for signal "{signal_id}" not found.')

    def _resolve_subscription_plan_group_for_target(
        self,
        handle: _VerificationRuntimeOrchestrationHandle,
        target_index: int,
    ):
        for group in handle.subscription_plan.groups:
            if int(target_index) in {int(index) for index in group.target_indexes}:
                return group
        return None

    def _resolve_runtime_report_for_group(
        self,
        handle: _VerificationRuntimeOrchestrationHandle,
        group_id: str,
    ):
        runtime_plan = build_runtime_subscription_plan(handle.subscription_plan)
        for device_group in group_report_subscription_plan_devices_by_endpoint(
            plan=runtime_plan,
            endpoint_for_device=handle.endpoint_for_device,
        ):
            session_state = next(
                (item for item in handle.session_states.values() if item.endpoint_id == device_group.endpoint.id),
                None,
            )
            if session_state is None:
                continue
            for device in device_group.devices:
                for report in device.reports:
                    if str(report.candidate.id) != str(group_id):
                        continue
                    subscription_id = _resolve_subscription_id_for_runtime(
                        session_id=session_state.session_id,
                        report=report,
                    )
                    subscription_state = handle.subscription_states.get(subscription_id)
                    if subscription_state is None:
                        raise Iec61850ReportRuntimeError(
                            "SUBSCRIPTION_NOT_FOUND",
                            f'IEC 61850 subscription "{subscription_id}" is not active.',
                        )
                    if subscription_state.subscription_state != "reporting":
                        raise Iec61850ReportRuntimeError(
                            "SUBSCRIPTION_NOT_REPORTING",
                            f'IEC 61850 subscription "{subscription_id}" is not reporting.',
                        )
                    return report, session_state, subscription_state
        raise Iec61850ReportRuntimeError(
            "REPORT_GROUP_NOT_FOUND",
            f'IEC 61850 report group "{group_id}" was not found in the active subscription plan.',
        )

    def _mark_group_capture_failed(
        self,
        handle: _VerificationRuntimeOrchestrationHandle,
        group_id: str,
        code: str,
        message: str,
    ) -> None:
        for subscription_state in handle.subscription_states.values():
            if subscription_state.group_id != group_id:
                continue
            subscription_state.report_health = "degraded"
            subscription_state.last_error = message
            subscription_state.diagnostic_code = code


def _runtime_diagnostics_to_evidence_diagnostics(
    diagnostics: Sequence[Iec61850RuntimeDiagnostic],
) -> list[VerificationEvidenceDiagnosticSchema]:
    return [
        VerificationEvidenceDiagnosticSchema(
            code=diagnostic.code,
            message=diagnostic.message,
            severity=diagnostic.severity,
            details={
                **(getattr(diagnostic, "details", None) or {}),
                "endpoint_id": diagnostic.reference.ied_name if diagnostic.reference is not None else None,
                "report_control_name": diagnostic.reference.report_control_name if diagnostic.reference is not None else None,
            },
        )
        for diagnostic in diagnostics
    ]


def _report_event_values_payload(event) -> list[dict[str, Any]]:
    values = getattr(event, "values", ()) or ()
    result: list[dict[str, Any]] = []
    for value in values:
        result.append(
            {
                "index": getattr(value, "data_set_index", None),
                "reference": getattr(value, "reference", None),
                "data_reference": getattr(value, "data_reference", None),
                "value": getattr(value, "value", None),
                "reason": getattr(getattr(value, "reason_code", None), "value", getattr(value, "reason_code", None)),
                "timestamp": getattr(value, "timestamp", None),
            }
        )
    return result


def _probe_mms_endpoint_reachability(endpoint: Iec61850DeviceEndpoint) -> tuple[bool, str | None]:
    host = str(endpoint.host or "").strip()
    if endpoint.mode.name != "MMS" or not host:
        return True, None
    try:
        with socket.create_connection(
            (host, int(endpoint.port)),
            timeout=_MMS_ORCHESTRATION_PREFLIGHT_TIMEOUT_SECONDS,
        ):
            return True, None
    except OSError as exc:
        return False, str(exc)


def _runtime_error_to_evidence_diagnostic(
    *,
    error: Iec61850ReportRuntimeError,
    endpoint: Iec61850DeviceEndpoint,
    report,
    session_id: str,
    severity: str = "error",
) -> VerificationEvidenceDiagnosticSchema:
    return VerificationEvidenceDiagnosticSchema(
        code=error.code,
        message=error.message,
        severity=severity,
        details={
            key: value
            for key, value in {
                "session_id": session_id,
                "endpoint_id": endpoint.id,
                "endpoint_host": endpoint.host,
                "endpoint_port": endpoint.port,
                "ied_name": endpoint.ied_name,
                "access_point_name": endpoint.access_point_name,
                "group_id": getattr(report.candidate, "id", None),
                "report_control_name": getattr(report.candidate, "report_control_name", None),
                "data_set_reference": getattr(report.candidate, "data_set_ref", None),
            }.items()
            if value is not None
        },
    )


def _session_failure_to_evidence_diagnostic(
    *,
    session_state: VerificationRuntimeSessionState,
    endpoint: Iec61850DeviceEndpoint,
    report,
) -> VerificationEvidenceDiagnosticSchema:
    return VerificationEvidenceDiagnosticSchema(
        code=session_state.diagnostic_code or "SESSION_FAILED",
        message=session_state.last_error or "IEC 61850 session is failed.",
        severity="error",
        details={
            key: value
            for key, value in {
                "session_id": session_state.session_id,
                "endpoint_id": endpoint.id,
                "endpoint_host": endpoint.host,
                "endpoint_port": endpoint.port,
                "ied_name": endpoint.ied_name,
                "access_point_name": endpoint.access_point_name,
                "group_id": getattr(report.candidate, "id", None),
                "report_control_name": getattr(report.candidate, "report_control_name", None),
                "data_set_reference": getattr(report.candidate, "data_set_ref", None),
            }.items()
            if value is not None
        },
    )


def _observation_diagnostics_to_evidence_diagnostics(
    diagnostics: Sequence[Any],
) -> list[VerificationEvidenceDiagnosticSchema]:
    return [
        VerificationEvidenceDiagnosticSchema(
            code=str(getattr(diagnostic, "code", "report_observation_diagnostic")),
            message=str(getattr(diagnostic, "message", "Report observation diagnostic")),
            severity=str(getattr(diagnostic, "severity", "info") or "info"),
            details={
                key: value
                for key, value in {
                    "signal_id": getattr(diagnostic, "signal_id", None),
                    "address": getattr(diagnostic, "address", None),
                    "data_reference": getattr(diagnostic, "data_reference", None),
                    "report_control_name": getattr(getattr(diagnostic, "reference", None), "report_control_name", None),
                }.items()
                if value is not None
            }
            or None,
        )
        for diagnostic in diagnostics
    ]


def _resolve_subscription_id_for_runtime(*, session_id: str, report) -> str:
    candidate_id = str(getattr(report.candidate, "id", "")).strip()
    if candidate_id:
        return f"{session_id}:{candidate_id}"
    report_control_name = str(getattr(report.candidate, "report_control_name", "")).strip()
    data_set_ref = str(getattr(report.candidate, "data_set_ref", "")).strip()
    if report_control_name:
        return f"{session_id}:{report_control_name}"
    if data_set_ref:
        return f"{session_id}:{data_set_ref}"
    return f"{session_id}:subscription"


def _candidate_report_reference(candidate: Any) -> str:
    for value in (
        getattr(candidate, "rpt_id", None),
        getattr(candidate, "report_control_name", None),
        _candidate_signal_scope(candidate),
        getattr(candidate, "id", None),
    ):
        text = str(value or "").strip()
        if text:
            return text
    return "discovery-only-report"


def _candidate_signal_scope(candidate: Any) -> str | None:
    logical_device = str(getattr(candidate, "logical_device_inst", "") or "").strip()
    logical_node = str(getattr(candidate, "logical_node_name", "") or "").strip()
    if logical_device and logical_node:
        return f"{logical_device}/{logical_node}"
    signals = getattr(candidate, "signals", ())
    for signal in signals or ():
        reference = str(getattr(signal, "reference", "") or "").strip()
        if not reference:
            continue
        without_fc = reference.split("[", 1)[0]
        if "/" in without_fc:
            domain, rest = without_fc.split("/", 1)
            node = rest.split(".", 1)[0].split("/", 1)[0]
            if domain and node:
                return f"{domain}/{node}"
        if "." in without_fc:
            return without_fc.split(".", 1)[0]
    return None


def _should_defer_startup_general_interrogation(endpoint: Iec61850DeviceEndpoint) -> bool:
    return False


def _runtime_summary_key(session_state: VerificationSessionSnapshotSchema) -> str:
    return f"{session_state.endpoint_id}:{session_state.runtime_state}"


def _build_runtime_summary_for_orchestration(
    *,
    session_snapshots: Sequence[VerificationSessionSnapshotSchema],
    subscription_snapshots: Sequence[VerificationSubscriptionSnapshotSchema],
    client_id: str,
    diagnostics: Sequence[VerificationEvidenceDiagnosticSchema],
) -> dict[str, Any]:
    summary = {
        "active_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state != "closed"),
        "reporting_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state == "reporting"),
        "failed_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state == "failed"),
        "closed_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state == "closed"),
        "active_subscriptions": sum(1 for snapshot in subscription_snapshots if snapshot.subscription_state != "closed"),
        "reporting_subscriptions": sum(
            1 for snapshot in subscription_snapshots if snapshot.subscription_state == "reporting"
        ),
        "failed_subscriptions": sum(1 for snapshot in subscription_snapshots if snapshot.subscription_state == "failed"),
        "client_id": client_id,
        "runtime_diagnostics": len(diagnostics),
        "session_states": [_runtime_summary_key(snapshot) for snapshot in session_snapshots],
    }
    return summary


def _build_recovery_state_for_orchestration(
    *,
    test_run_id: str,
    verification_targets: Sequence[VerificationTargetSchema],
    subscription_plan: VerificationSubscriptionPlanSchema,
    execution_context: VerificationExecutionContextSchema,
    session_snapshots: Sequence[VerificationSessionSnapshotSchema],
    subscription_snapshots: Sequence[VerificationSubscriptionSnapshotSchema],
) -> VerificationRecoveryStateSchema | None:
    if not session_snapshots and not subscription_snapshots:
        return None

    if session_snapshots and all(snapshot.runtime_state == "closed" for snapshot in session_snapshots) and all(
        snapshot.subscription_state == "closed" for snapshot in subscription_snapshots
    ):
        return None

    if all(snapshot.runtime_state == "reporting" for snapshot in session_snapshots) and all(
        snapshot.subscription_state in {"reporting", "enabled"} for snapshot in subscription_snapshots
    ):
        return None

    representative_session = next(
        (snapshot for snapshot in session_snapshots if snapshot.runtime_state != "reporting"),
        session_snapshots[0] if session_snapshots else None,
    )
    representative_subscription = next(
        (snapshot for snapshot in subscription_snapshots if snapshot.subscription_state not in {"reporting", "enabled"}),
        subscription_snapshots[0] if subscription_snapshots else None,
    )
    if any(snapshot.runtime_state == "reconnecting" for snapshot in session_snapshots) or any(
        snapshot.subscription_state == "reconnecting" for snapshot in subscription_snapshots
    ):
        desired_state = "reconnecting"
        recovery_reason = "user_reconnect"
    elif any(snapshot.runtime_state in {"connecting", "discovering"} for snapshot in session_snapshots):
        desired_state = "discovering"
        recovery_reason = None
    elif any(snapshot.subscription_state in {"pending", "reserving", "enabled"} for snapshot in subscription_snapshots):
        desired_state = "subscribing"
        recovery_reason = None
    elif any(snapshot.runtime_state == "degraded" for snapshot in session_snapshots) or any(
        snapshot.subscription_state == "degraded" for snapshot in subscription_snapshots
    ):
        desired_state = "reconnecting"
        recovery_reason = "report_health_degraded"
    elif any(snapshot.runtime_state == "failed" for snapshot in session_snapshots) or any(
        snapshot.subscription_state == "failed" for snapshot in subscription_snapshots
    ):
        desired_state = "reconnecting"
        recovery_reason = "runtime_failure"
    else:
        desired_state = "reporting"
        recovery_reason = None

    recovery_diagnostics: list[VerificationEvidenceDiagnosticSchema] = []
    if representative_session is not None and representative_session.last_error is not None:
        recovery_diagnostics.append(
            VerificationEvidenceDiagnosticSchema(
                code=representative_session.diagnostic_code or "SESSION_RECOVERY",
                message=representative_session.last_error,
                severity="warning" if representative_session.runtime_state != "failed" else "error",
                details={
                    "session_id": representative_session.session_id,
                    "endpoint_id": representative_session.endpoint_id,
                },
            )
        )
    if representative_subscription is not None and representative_subscription.last_error is not None:
        recovery_diagnostics.append(
            VerificationEvidenceDiagnosticSchema(
                code=representative_subscription.diagnostic_code or "SUBSCRIPTION_RECOVERY",
                message=representative_subscription.last_error,
                severity="warning" if representative_subscription.subscription_state != "failed" else "error",
                details={
                    "subscription_id": representative_subscription.subscription_id,
                    "session_id": representative_subscription.session_id,
                    "endpoint_id": representative_subscription.endpoint_id,
                },
            )
        )

    return VerificationRecoveryStateSchema(
        session_id=representative_session.session_id if representative_session is not None else f"{test_run_id}:recovery",
        endpoint_id=representative_session.endpoint_id if representative_session is not None else execution_context.selected_group_id or test_run_id,
        runtime_state=_recovery_runtime_state(session_snapshots, subscription_snapshots),
        desired_state=desired_state,
        active_generation=max((snapshot.connection_generation for snapshot in session_snapshots), default=1),
        recovery_reason=recovery_reason,
        desired_subscription_plan_id=subscription_plan.plan_id or test_run_id,
        desired_group_ids=_unique_non_empty_strings(
            [
                execution_context.selected_group_id,
                *(group.group_id for group in subscription_plan.groups),
            ]
        ),
        desired_report_controls=_unique_non_empty_strings(
            [
                group.report_control_reference or group.rpt_id or group.report_control_name or group.group_id
                for group in subscription_plan.groups
            ]
        ),
        desired_target_ids=[int(target.signal_id) for target in verification_targets],
        active_verification_run_id=test_run_id,
        preserved_evidence_count=0,
        in_flight=desired_state != "reporting",
        preserved_verification_targets=list(verification_targets),
        stale_signal_count=sum(
            1
            for snapshot in subscription_snapshots
            if snapshot.subscription_state in {"reconnecting", "degraded", "failed"}
            or snapshot.report_health == "degraded"
        ),
        diagnostics=recovery_diagnostics,
    )


def _recovery_runtime_state(
    session_snapshots: Sequence[VerificationSessionSnapshotSchema],
    subscription_snapshots: Sequence[VerificationSubscriptionSnapshotSchema],
) -> str:
    runtime_state = _resolve_runtime_state(session_snapshots, subscription_snapshots) or "reporting"
    if runtime_state == "connecting":
        return "discovering"
    return runtime_state
