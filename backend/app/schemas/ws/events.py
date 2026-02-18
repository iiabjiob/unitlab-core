from datetime import datetime
from typing import Any, Dict, List, Literal, Union

from pydantic import BaseModel
from app.infrastructure.protocol.modes import State
from app.infrastructure.protocol.packet_structures import RespStatus, RespError
from app.schemas.device_schema import DeviceSchema
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

class SequenceEventBase(BaseModel):
    topic: Literal["sequence"] = "sequence"
    sequence_id: int
    run_id: int


class SequenceStartedEvent(SequenceEventBase):
    event: Literal["started"] = "started"
    total_steps: int


class SequenceProgressEvent(SequenceEventBase):
    event: Literal["progress"] = "progress"
    step_index: int
    step_id: int
    step_type: str
    step_elapsed_ms: int
    run_elapsed_ms: int
    completed_steps: List[int]


class SequenceStepErrorEvent(SequenceEventBase):
    event: Literal["step_error"] = "step_error"
    step_index: int
    step_id: int
    message: str


class SequenceErrorEvent(SequenceEventBase):
    event: Literal["error"] = "error"
    message: str


class SequenceStoppingEvent(SequenceEventBase):
    event: Literal["stopping"] = "stopping"
    current_step_index: int
    total_steps: int


class SequenceStoppedEvent(SequenceEventBase):
    event: Literal["stopped"] = "stopped"


class SequenceCompletedEvent(SequenceEventBase):
    event: Literal["completed"] = "completed"
    elapsed_ms: int


class SystemHealthChangedEvent(BaseModel):
    channel: Literal[WSChannel.SYSTEM_INFO] = WSChannel.SYSTEM_INFO
    event: Literal["system_health_changed"] = "system_health_changed"
    previous_status: Literal["online", "degraded", "offline"] | None = None
    current_status: Literal["online", "degraded", "offline"]
    changed_at: datetime
    issues: List[str]
    diff: Dict[str, List[str]]
    snapshot: Dict[str, Any]


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
    SignalAllocationJobEvent,
]
