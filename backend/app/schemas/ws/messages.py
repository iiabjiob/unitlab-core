from pydantic import BaseModel
from typing import Literal, Union, Optional
from app.infrastructure.protocol.modes import Cmd
from app.infrastructure.protocol.modes import State
from enum import Enum

class WSAction(str, Enum):
    SUBSCRIBE       = "subscribe"
    UNSUBSCRIBE     = "unsubscribe"
    SET_DO_COMMAND  = "set_do_command"
    SET_AO_COMMAND  = "set_ao_command"
    GET_STATES      = "get_states"
    SCAN_DEVICES    = "scan_devices"

# ---------------------------------------------------------------------
# Подписка на каналы
# ---------------------------------------------------------------------

class WsSubscribeMessage(BaseModel):
    action: Literal[WSAction.SUBSCRIBE]
    channels: list[str]


class WsUnsubscribeMessage(BaseModel):
    action: Literal[WSAction.UNSUBSCRIBE]
    channels: list[str]


# ---------------------------------------------------------------------
# Управление выходами (DO / AO)
# ---------------------------------------------------------------------

class SetDoCommandMessage(BaseModel):
    """
    Управление цифровыми выходами (DO).
    mode → соответствует protocol.modes.Cmd:
        - SET_SINGLE_BIT
        - SET_ALL_BIT
        - SET_PAIR_BIT
    """
    action: Literal[WSAction.SET_DO_COMMAND]
    unit_id: str
    mode: Cmd
    ch: Optional[int] = None
    value: Optional[int] = None
    bitmask: Optional[int] = None
    chA: Optional[int] = None
    chB: Optional[int] = None
    state2b: Optional[int] = None


class SetAoCommandMessage(BaseModel):
    """
    Управление аналоговыми выходами (AO).
    """
    action: Literal[WSAction.SET_AO_COMMAND]
    unit_id: str
    ch: int
    value: float


# ---------------------------------------------------------------------
# Запросы состояния (DI / DO / AO)
# ---------------------------------------------------------------------

class RequestStateMessage(BaseModel):
    """
    Запрос состояния устройства.
    mode → соответствует protocol.modes.State:
        - REQ_SINGLE_BIT / REQ_ALL_BIT
        - REQ_SINGLE_FLOAT / REQ_ALL_FLOAT
    """
    action: Literal[WSAction.GET_STATES]
    unit_id: str
    device_type: str  # "di" | "do" | "ao"
    mode: State
    ch: Optional[int] = None


# ---------------------------------------------------------------------
# Сканирование устройств
# ---------------------------------------------------------------------

class ScanDevicesMessage(BaseModel):
    action: Literal[WSAction.SCAN_DEVICES]


# ---------------------------------------------------------------------
# Унифицированный union
# ---------------------------------------------------------------------

WSMessage = Union[
    WsSubscribeMessage,
    WsUnsubscribeMessage,
    SetDoCommandMessage,
    SetAoCommandMessage,
    RequestStateMessage,
    ScanDevicesMessage,
]
