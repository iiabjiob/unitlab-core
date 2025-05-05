from pydantic import BaseModel
from typing import Literal, Union

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

class SetGroupMessage(BaseModel):
    action: Literal["set_group"]
    unitId: str
    group: list[int]

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
