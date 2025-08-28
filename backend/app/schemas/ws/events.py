from pydantic import BaseModel
from typing import Literal, Union, Dict, Any
from app.infrastructure.protocol.modes import State
from app.infrastructure.protocol.packet_structures import RespStatus, RespError
from app.schemas.device import DeviceOut
from enum import Enum

class WSChannel(str, Enum):
    DEVICE_STATE    = "devices/state"
    DEVICE_REGISTER = "devices/register"
    DEVICE_RESP     = "devices/resp"
    DEVICE_STATUS   = "devices/status"
    TIME_STATUS     = "time/status"


# ---------------------------------------------------------------------
# Состояния (DI/DO/AO)
# ---------------------------------------------------------------------

class DeviceStateEvent(BaseModel):
    channel: Literal[WSChannel.DEVICE_STATE] = WSChannel.DEVICE_STATE
    unit_id: str
    device_type: str
    timestamp: int
    mode: State                   # Enum из protocol.modes
    payload: Dict[str, Any]

    class Config:
        use_enum_values = False   # сериализация Enum → .name (строка)


# ---------------------------------------------------------------------
# Регистрация устройства
# ---------------------------------------------------------------------

class DeviceRegisterEvent(DeviceOut):
    channel: Literal[WSChannel.DEVICE_REGISTER] = WSChannel.DEVICE_REGISTER


# ---------------------------------------------------------------------
# RESP (ответы на команды)
# ---------------------------------------------------------------------

class DeviceRespEvent(BaseModel):
    channel: Literal[WSChannel.DEVICE_RESP] = WSChannel.DEVICE_RESP
    unit_id: str
    device_type: str
    packet_id: int
    status: RespStatus
    error: RespError
    timestamp: int

    class Config:
        use_enum_values = False  # сериализация как .name


# ---------------------------------------------------------------------
# Heartbeat
# ---------------------------------------------------------------------

class DeviceHeartbeatEvent(BaseModel):
    channel: Literal[WSChannel.DEVICE_STATUS] = WSChannel.DEVICE_STATUS
    unit_id: str
    device_type: str
    status: Literal["online", "offline"]
    last_seen: int


class TimeStatusEvent(BaseModel):
    channel: Literal[WSChannel.TIME_STATUS] = WSChannel.TIME_STATUS
    timestamp: str            # ISO8601 строка (UTC)
    status: str               # напр. "synced" / "unsynced"
    source: str | None = None
    offset_us: int | None = None

# ---------------------------------------------------------------------
# Union для всех событий
# ---------------------------------------------------------------------

WSEvent = Union[
    DeviceStateEvent,
    DeviceRegisterEvent,
    DeviceRespEvent,
    DeviceHeartbeatEvent,
    TimeStatusEvent,
]
