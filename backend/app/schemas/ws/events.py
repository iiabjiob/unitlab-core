from pydantic import BaseModel
from typing import Literal, Union, Dict, Any, Optional, List
from app.infrastructure.protocol.modes import State
from app.infrastructure.protocol.packet_structures import RespStatus, RespError
from app.schemas.device_schema import DeviceSchema
from app.schemas.time import TimeStatus
from enum import Enum

# -----------------------------------------------------------------
    # NOTE:
    # Каналы WebSocket намеренно названы во множественном числе ("devices/..."),
    # чтобы сохранить консистентность с MQTT-топиками:
    #
    #   MQTT:    devices/+/state
    #   WS:      devices/state
    #
    # В обоих случаях это означает "события для множества устройств",
    # а само сообщение внутри содержит unit_id, type и т.д.
    #
    # Даже если WebSocket сообщение всегда описывает одно устройство,
    # мы НЕ переключаемся на "device/...", чтобы не плодить два разных
    # пространства имен для одинаковых событий.
    # -----------------------------------------------------------------
class WSChannel(str, Enum):
    SYSTEM_INFO     = "system/info"
    TIME_STATUS     = "time/status"

    DEVICE_STATE    = "devices/state"
    DEVICE_REGISTER = "devices/register"
    DEVICE_RESP     = "devices/resp"
    DEVICE_STATUS   = "devices/status"
    EVENT_LOG = "events/log"

class EventDirection(str, Enum):
    IN = "IN"
    OUT = "OUT"

class EventSource(str, Enum):
    WS_DEVICE = "WS_DEVICE"
    WS_COMMAND = "WS_COMMAND"

# ---------------------------------------------------------------------
# Состояния (DI/DO/AO)
# ---------------------------------------------------------------------

class DeviceStateEvent(BaseModel):
    channel: Literal[WSChannel.DEVICE_STATE] = WSChannel.DEVICE_STATE
    unit_id: str
    timestamp: int
    mode: State                   # Enum из protocol.modes
    payload: Dict[str, Any]

# ---------------------------------------------------------------------
# Регистрация устройства
# ---------------------------------------------------------------------

class DeviceRegisterEvent(DeviceSchema):
    channel: Literal[WSChannel.DEVICE_REGISTER] = WSChannel.DEVICE_REGISTER
    created: bool = False

# ---------------------------------------------------------------------
# RESP (ответы на команды)
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

# ---------------------------------------------------------------------
# TimeStatus
# ---------------------------------------------------------------------

class TimeStatusEvent(TimeStatus):
    channel: Literal[WSChannel.TIME_STATUS] = WSChannel.TIME_STATUS

# ---------------------------------------------------------------------
# EventLog
# ---------------------------------------------------------------------
class EventLogEvent(BaseModel):
    channel: Literal[WSChannel.EVENT_LOG] = WSChannel.EVENT_LOG
    id: str
    ts: int
    dir: EventDirection
    source: EventSource
    channel_or_action: str
    unit_id: Optional[str]
    type: Optional[str]
    summary: str
    payload: Optional[Any]

    model_config = {
        "json_encoders": {
            EventDirection: lambda v: v.name,
            EventSource: lambda v: v.name,
        }
    }


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
    elapsed_ms: int
    completed_steps: List[int]


class SequenceStepErrorEvent(SequenceEventBase):
    event: Literal["step_error"] = "step_error"
    step_index: int
    step_id: int
    message: str


class SequenceErrorEvent(SequenceEventBase):
    event: Literal["error"] = "error"
    message: str


class SequenceStoppedEvent(SequenceEventBase):
    event: Literal["stopped"] = "stopped"


class SequenceCompletedEvent(SequenceEventBase):
    event: Literal["completed"] = "completed"
    elapsed_ms: int

# ---------------------------------------------------------------------
# Union для всех событий
# ---------------------------------------------------------------------

WSEvent = Union[
    DeviceStateEvent,
    DeviceRegisterEvent,
    DeviceRespEvent,
    DeviceHeartbeatEvent,
    TimeStatusEvent,
    EventLogEvent,
    SequenceStartedEvent,
    SequenceProgressEvent,
    SequenceStepErrorEvent,
    SequenceErrorEvent,
    SequenceStoppedEvent,
    SequenceCompletedEvent,
]
