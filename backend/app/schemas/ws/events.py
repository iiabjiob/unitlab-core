from pydantic import BaseModel, field_serializer
from typing import Literal, Union, Dict, Any
from app.infrastructure.protocol.modes import State
from app.infrastructure.protocol.packet_structures import RespStatus, RespError
from app.schemas.device import DeviceOut
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
    # а само сообщение внутри содержит unit_id, device_type и т.д.
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
    DEVICE_STATUS   = "devices/status"  # online/offline heartbeat

# ---------------------------------------------------------------------
# Динамические каналы (по устройствам)
# ---------------------------------------------------------------------
def device_state(device_type: str, unit_id: str) -> str:
    return f"devices/{device_type}/{unit_id}/state"

# ---------------------------------------------------------------------
# Event модели
# ---------------------------------------------------------------------

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

# ---------------------------------------------------------------------
# TimeStatus
# ---------------------------------------------------------------------

class TimeStatusEvent(TimeStatus):
    channel: Literal[WSChannel.TIME_STATUS] = WSChannel.TIME_STATUS

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
