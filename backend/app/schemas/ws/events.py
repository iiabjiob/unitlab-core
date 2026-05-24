from datetime import datetime
from typing import Any, Dict, List, Literal, Union

from pydantic import BaseModel, Field
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
    status: RespStatus
    error: RespError
    timestamp: int

    model_config = {
        "json_encoders": {
            RespStatus: lambda v: v.name,
            RespError: lambda v: v.name,
        }
    }

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
