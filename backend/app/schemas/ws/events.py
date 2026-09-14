from datetime import datetime
from typing import Any, Dict, List, Literal, Union

from pydantic import BaseModel, Field, field_serializer
from app.infrastructure.protocol.modes import State
from app.infrastructure.protocol.packet_structures import RespStatus, RespError
from app.schemas.device_schema import DeviceSchema
from app.schemas.sequence_run_schema import SequenceRuntimeSchema
from enum import Enum

# -----------------------------------------------------------------
    # NOTE:
    # WebSocket channels intentionally use plural names ("devices/...")
    # to stay aligned with the MQTT topics:
    #
    #   MQTT:    devices/+/state
    #   WS:      devices/state
    #
    # Both mean "events for multiple devices" while the payload carries
    # unit_id, type, etc.
    #
    # Even though each WS message describes a single device, we keep
    # the plural naming to avoid maintaining two namespaces.
    # -----------------------------------------------------------------
class WSChannel(str, Enum):
    SYSTEM_INFO     = "system/info"
    TIME_STATUS     = "time/status"

    DEVICE_STATE    = "devices/state"
    DEVICE_REGISTER = "devices/register"
    DEVICE_RESP     = "devices/resp"
    DEVICE_STATUS   = "devices/status"
    EXTERNAL_IED_STATUS = "external-ieds/status"
    EXTERNAL_IED_MANUAL_REPORTS = "external-ieds/manual-reports"

# ---------------------------------------------------------------------
# Device states (DI/DO/AO)
# ---------------------------------------------------------------------

class DeviceStateEvent(BaseModel):
    channel: Literal[WSChannel.DEVICE_STATE] = WSChannel.DEVICE_STATE
    unit_id: str
    timestamp: int
    mode: State                   # Enum from protocol.modes
    payload: Dict[str, Any]

# ---------------------------------------------------------------------
# Device registration
# ---------------------------------------------------------------------

class DeviceRegisterEvent(DeviceSchema):
    channel: Literal[WSChannel.DEVICE_REGISTER] = WSChannel.DEVICE_REGISTER
    created: bool = False

# ---------------------------------------------------------------------
# RESP (command responses)
# ---------------------------------------------------------------------

class DeviceRespEvent(BaseModel):
    channel: Literal[WSChannel.DEVICE_RESP] = WSChannel.DEVICE_RESP
    unit_id: str
    packet_id: int
    command_id: str | None = None
    status: RespStatus
    error: RespError
    timestamp: int

    @field_serializer("status", "error")
    def _serialize_resp_enum(self, value: RespStatus | RespError) -> str:
        return value.name

# ---------------------------------------------------------------------
# Heartbeat
# ---------------------------------------------------------------------

class DeviceHeartbeatEvent(BaseModel):
    channel: Literal[WSChannel.DEVICE_STATUS] = WSChannel.DEVICE_STATUS
    unit_id: str
    status: Literal["online", "offline"]
    last_seen: int
    heartbeat_kind: Literal["fast", "diag"] | None = None
    heartbeat_fast: Dict[str, Any] | None = None
    heartbeat_diag: Dict[str, Any] | None = None


ExternalIedStatus = Literal["not_applicable", "unknown", "expected", "reachable", "offline"]
ExternalIedCheckKind = Literal["none", "tcp_connect"]
ExternalIedFailureCode = Literal["unreachable", "mms_unavailable", "network_unreachable", "probe_failed"]
ExternalIedDiscoveryState = Literal["NeverDiscovered", "Queued", "Running", "Succeeded", "Failed", "RetryWaiting", "Cancelled", "Stale"]
ExternalIedPlanningState = Literal["NotPlanned", "Queued", "Running", "Ready", "Partial", "Failed", "Stale", "WaitingForDiscovery"]
ExternalIedPlanningSignalStatus = Literal["matched", "unmatched", "ambiguous", "not_planned", "stale"]


class ExternalIedStatusRecord(BaseModel):
    ip: str
    port: int = 102
    status: ExternalIedStatus
    signal_ids: list[int] = Field(default_factory=list)
    last_checked_at: str | None = None
    last_error: str | None = None
    check_kind: ExternalIedCheckKind = "none"
    failure_code: ExternalIedFailureCode | None = None
    discovery_state: ExternalIedDiscoveryState = "NeverDiscovered"
    discovery_retry_at_ms: int | None = None
    discovery_last_error: str | None = None
    discovery_updated_at_ms: int | None = None
    discovery_ready_for_verification: bool = False
    discovery_device_identity: str | None = None
    discovery_vendor: str | None = None
    discovery_model: str | None = None
    discovery_datasets: int | None = None
    discovery_rcbs: int | None = None
    discovery_model_signals: int | None = None
    planning_state: ExternalIedPlanningState = "NotPlanned"
    planning_updated_at_ms: int | None = None
    planning_matched_count: int = 0
    planning_unmatched_count: int = 0
    planning_ambiguous_count: int = 0
    planning_last_error: str | None = None


class ExternalIedStatusSnapshotEvent(BaseModel):
    channel: Literal[WSChannel.EXTERNAL_IED_STATUS] = WSChannel.EXTERNAL_IED_STATUS
    event: Literal["external_ied_status_snapshot"] = "external_ied_status_snapshot"
    workspace_id: int
    devices: list[ExternalIedStatusRecord] = Field(default_factory=list)
    removed_signal_ids: list[int] = Field(default_factory=list)
    emitted_at: str


class ExternalIedStatusChangedEvent(BaseModel):
    channel: Literal[WSChannel.EXTERNAL_IED_STATUS] = WSChannel.EXTERNAL_IED_STATUS
    event: Literal["external_ied_status_changed"] = "external_ied_status_changed"
    workspace_id: int
    ip: str
    port: int = 102
    old_status: ExternalIedStatus
    new_status: ExternalIedStatus
    signal_ids: list[int] = Field(default_factory=list)
    checked_at: str
    check_kind: ExternalIedCheckKind
    failure_code: ExternalIedFailureCode | None = None
    error: str | None = None
    discovery_state: ExternalIedDiscoveryState = "NeverDiscovered"
    discovery_retry_at_ms: int | None = None
    discovery_last_error: str | None = None
    discovery_updated_at_ms: int | None = None
    discovery_ready_for_verification: bool = False
    discovery_device_identity: str | None = None
    discovery_vendor: str | None = None
    discovery_model: str | None = None
    discovery_datasets: int | None = None
    discovery_rcbs: int | None = None
    discovery_model_signals: int | None = None
    planning_state: ExternalIedPlanningState = "NotPlanned"
    planning_updated_at_ms: int | None = None
    planning_matched_count: int = 0
    planning_unmatched_count: int = 0
    planning_ambiguous_count: int = 0
    planning_last_error: str | None = None


class ExternalIedPlanningSignalResult(BaseModel):
    signal_id: int
    endpoint: str
    status: ExternalIedPlanningSignalStatus
    address: str | None = None
    reason: str | None = None
    ied_identity: str | None = None
    fcda_reference: str | None = None
    dataset_reference: str | None = None
    rcb_reference: str | None = None
    rcb_name: str | None = None


class ExternalIedPlanningEndpointRecord(BaseModel):
    endpoint: str
    ip: str
    port: int = 102
    state: ExternalIedPlanningState = "NotPlanned"
    planning_fingerprint: str | None = None
    model_fingerprint: str | None = None
    updated_at_ms: int | None = None
    matched_count: int = 0
    unmatched_count: int = 0
    ambiguous_count: int = 0
    signal_ids: list[int] = Field(default_factory=list)
    last_error: str | None = None


class ExternalIedPlanningSnapshotEvent(BaseModel):
    channel: Literal[WSChannel.EXTERNAL_IED_STATUS] = WSChannel.EXTERNAL_IED_STATUS
    event: Literal["external_ied_planning_snapshot"] = "external_ied_planning_snapshot"
    workspace_id: int
    endpoints: list[ExternalIedPlanningEndpointRecord] = Field(default_factory=list)
    signal_results: list[ExternalIedPlanningSignalResult] = Field(default_factory=list)
    removed_signal_ids: list[int] = Field(default_factory=list)
    emitted_at: str


class ExternalIedPlanningChangedEvent(BaseModel):
    channel: Literal[WSChannel.EXTERNAL_IED_STATUS] = WSChannel.EXTERNAL_IED_STATUS
    event: Literal["external_ied_planning_changed"] = "external_ied_planning_changed"
    workspace_id: int
    endpoint: ExternalIedPlanningEndpointRecord
    signal_results: list[ExternalIedPlanningSignalResult] = Field(default_factory=list)
    removed_signal_ids: list[int] = Field(default_factory=list)
    emitted_at: str


class ExternalIedManualReportValuesChangedEvent(BaseModel):
    channel: Literal[WSChannel.EXTERNAL_IED_MANUAL_REPORTS] = WSChannel.EXTERNAL_IED_MANUAL_REPORTS
    event: Literal["external_ied_manual_report_values_changed"] = "external_ied_manual_report_values_changed"
    workspace_id: int
    endpoint: str
    ip: str
    port: int = 102
    report_reference: str
    lease_id: str
    status: str
    signal_states: list[dict[str, Any]] = Field(default_factory=list)
    report_values: list[dict[str, Any]] = Field(default_factory=list)
    emitted_at: str

class SequenceEventBase(BaseModel):
    topic: Literal["sequence"] = "sequence"
    sequence_id: int
    run_id: int


class SequenceStartedEvent(SequenceEventBase):
    event: Literal["started"] = "started"
    total_steps: int
    runtime: SequenceRuntimeSchema | None = None


class SequenceProgressEvent(SequenceEventBase):
    event: Literal["progress"] = "progress"
    step_index: int
    step_id: int
    step_type: str
    progress_scope: Literal["step", "nested_step"] = "step"
    step_elapsed_ms: int
    run_elapsed_ms: int
    completed_steps: List[int]
    runtime: SequenceRuntimeSchema | None = None


class SequenceStepErrorEvent(SequenceEventBase):
    event: Literal["step_error"] = "step_error"
    step_index: int
    step_id: int
    message: str
    runtime: SequenceRuntimeSchema | None = None


class SequenceErrorEvent(SequenceEventBase):
    event: Literal["error"] = "error"
    message: str
    runtime: SequenceRuntimeSchema | None = None


class SequenceStoppingEvent(SequenceEventBase):
    event: Literal["stopping"] = "stopping"
    current_step_index: int
    total_steps: int
    runtime: SequenceRuntimeSchema | None = None


class SequenceStoppedEvent(SequenceEventBase):
    event: Literal["stopped"] = "stopped"
    runtime: SequenceRuntimeSchema | None = None


class SequenceCompletedEvent(SequenceEventBase):
    event: Literal["completed"] = "completed"
    elapsed_ms: int
    runtime: SequenceRuntimeSchema | None = None


class SystemHealthChangedEvent(BaseModel):
    channel: Literal[WSChannel.SYSTEM_INFO] = WSChannel.SYSTEM_INFO
    event: Literal["system_health_changed"] = "system_health_changed"
    previous_status: Literal["online", "degraded", "offline"] | None = None
    current_status: Literal["online", "degraded", "offline"]
    changed_at: datetime
    issues: List[str]
    diff: Dict[str, List[str]]
    snapshot: Dict[str, Any]


class CoreNetworkStateEvent(BaseModel):
    channel: Literal[WSChannel.SYSTEM_INFO] = WSChannel.SYSTEM_INFO
    event: Literal["core_network_state"] = "core_network_state"
    snapshot: Dict[str, Any]
    changed_at: datetime


class CoreNtpStateEvent(BaseModel):
    channel: Literal[WSChannel.SYSTEM_INFO] = WSChannel.SYSTEM_INFO
    event: Literal["core_ntp_state"] = "core_ntp_state"
    snapshot: Dict[str, Any]
    changed_at: datetime


class CoreDiagnosticsStateEvent(BaseModel):
    channel: Literal[WSChannel.SYSTEM_INFO] = WSChannel.SYSTEM_INFO
    event: Literal["core_diagnostics_state"] = "core_diagnostics_state"
    snapshot: Dict[str, Any]
    changed_at: datetime


class CoreProvisionStateEvent(BaseModel):
    channel: Literal[WSChannel.SYSTEM_INFO] = WSChannel.SYSTEM_INFO
    event: Literal["core_provision_state"] = "core_provision_state"
    snapshot: Dict[str, Any]
    changed_at: datetime


class SignalAllocationJobEvent(BaseModel):
    channel: Literal[WSChannel.SYSTEM_INFO] = WSChannel.SYSTEM_INFO
    event: Literal["signal_allocation_job"] = "signal_allocation_job"
    job_id: str
    workspace_id: int
    operation: str
    status: Literal["queued", "running", "paused", "cancelling", "cancelled", "succeeded", "failed"]
    progress_total: int = 0
    progress_done: int = 0
    message: str | None = None
    error: str | None = None
    result: Dict[str, Any] = {}
    created_at: datetime
    updated_at: datetime


class SignalTestRunJobEvent(BaseModel):
    channel: Literal[WSChannel.SYSTEM_INFO] = WSChannel.SYSTEM_INFO
    event: Literal["signal_test_run_job"] = "signal_test_run_job"
    job_id: str
    workspace_id: int
    operation: str
    status: Literal["queued", "running", "paused", "cancelling", "cancelled", "succeeded", "failed"]
    progress_total: int = 0
    progress_done: int = 0
    message: str | None = None
    error: str | None = None
    result: Dict[str, Any] = {}
    created_at: datetime
    updated_at: datetime


class SignalTestRuntimePatchEvent(BaseModel):
    channel: Literal[WSChannel.SYSTEM_INFO] = WSChannel.SYSTEM_INFO
    event: Literal["signal_test_runtime_patch"] = "signal_test_runtime_patch"
    job_id: str
    workspace_id: int
    patch_type: Literal["tested_at"] = "tested_at"
    tested_at_by_signal: Dict[int, str] = Field(default_factory=dict)
    test_status_by_signal: Dict[int, str] = Field(default_factory=dict)
    emitted_at: datetime


SignalRowsPatchSource = Literal["allocation", "test_runtime", "device_health"]


class SignalRowsPatchedRowPatch(BaseModel):
    row_id: str | None = None
    signal_id: int
    changes: Dict[str, Any] = Field(default_factory=dict)
    columns: List[str] | None = None


class SignalRowsPatchedEvent(BaseModel):
    channel: Literal[WSChannel.SYSTEM_INFO] = WSChannel.SYSTEM_INFO
    event: Literal["signal_rows_patched"] = "signal_rows_patched"
    workspace_id: int
    sequence: int
    source: SignalRowsPatchSource
    patches: List[SignalRowsPatchedRowPatch] = Field(default_factory=list)
    requires_full_reload: bool = False
    emitted_at: datetime


def build_signal_job_event(job_state: Dict[str, Any]) -> SignalAllocationJobEvent | SignalTestRunJobEvent:
    operation = str(job_state.get("operation") or "").strip().lower()
    if operation == "test_run":
        return SignalTestRunJobEvent(**job_state)
    return SignalAllocationJobEvent(**job_state)

# ---------------------------------------------------------------------
# Union of all WS events
# ---------------------------------------------------------------------

WSEvent = Union[
    DeviceStateEvent,
    DeviceRegisterEvent,
    DeviceRespEvent,
    DeviceHeartbeatEvent,
    SequenceStartedEvent,
    SequenceProgressEvent,
    SequenceStepErrorEvent,
    SequenceErrorEvent,
    SequenceStoppingEvent,
    SequenceStoppedEvent,
    SequenceCompletedEvent,
    SystemHealthChangedEvent,
    CoreNetworkStateEvent,
    CoreNtpStateEvent,
    CoreDiagnosticsStateEvent,
    CoreProvisionStateEvent,
    SignalAllocationJobEvent,
    SignalTestRunJobEvent,
    SignalTestRuntimePatchEvent,
    SignalRowsPatchedEvent,
]
