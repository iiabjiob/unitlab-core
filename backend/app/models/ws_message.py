from pydantic import BaseModel
from typing import Literal, Union, List

# Тип действия в группе
class GroupAction(BaseModel):
    index: int
    state: bool
    delay_ms: int = 0
    is_pulse: bool = False
    pulse_duration: int = 200
    
# Подписка
class WsSubscribeMessage(BaseModel):
    action: Literal["subscribe"]
    channels: list[str]

class WsUnsubscribeMessage(BaseModel):
    action: Literal["unsubscribe"]
    channels: list[str]

# Управление устройствами
class SetPinMessage(BaseModel):
    action: Literal["set_pin"]
    unitId: str
    index: int
    value: bool
    value: bool
    delay_ms: int
    is_pulse: bool
    pulse_duration: int

class SetGroupMessage(BaseModel):
    action: Literal["set_group"]
    unitId: str
    actions: List[GroupAction]

# Команды системы
class ScanDevicesMessage(BaseModel):
    action: Literal["scan_devices"]

class RequestStatesMessage(BaseModel):
    action: Literal["request_states"]
    unitId: str

# Унифицированное сообщение
WSMessage = Union[
    WsSubscribeMessage,
    WsUnsubscribeMessage,
    SetPinMessage,
    SetGroupMessage,
    ScanDevicesMessage,
    RequestStatesMessage,
]
