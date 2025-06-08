from pydantic import BaseModel, Field
from typing import Literal, Union
    
# Подписка
class WsSubscribeMessage(BaseModel):
    action: Literal["subscribe"]
    channels: list[str]

class WsUnsubscribeMessage(BaseModel):
    action: Literal["unsubscribe"]
    channels: list[str]

# Управление устройствами
class SetDoCommandMessage(BaseModel):
    action: Literal["set_do_command"]
    unit_id: str
    mode: int = Field(..., description="0x04=latch, 0x01=pulse")
    delay_before_ms: int = Field(..., description="Delay before, ms")
    pulse_ms: int = Field(..., description="Pulse duration, ms (ignored for latch)")
    repeat: int = Field(..., description="Repeat count (ignored for latch)")
    bitmask: int = Field(..., description="Which DO (bitmask, 4 bytes unsigned)")

class RequestStateMessage(BaseModel):
    action: Literal["get_states"]
    unit_id: str
    type: str  # "do", "di", "ao"

# Команды системы
class ScanDevicesMessage(BaseModel):
    action: Literal["scan_devices"]

# Унифицированное сообщение
WSMessage = Union[
    WsSubscribeMessage,
    WsUnsubscribeMessage,
    SetDoCommandMessage,
    RequestStateMessage,
    ScanDevicesMessage,
]
