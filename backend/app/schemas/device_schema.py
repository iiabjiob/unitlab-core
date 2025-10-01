from pydantic import BaseModel, ConfigDict
from typing import List, Optional, Literal
from app.schemas.channel_schema import ChannelSchema

class DeviceSchema(BaseModel):
    id: int
    unit_id: str
    type: str
    num_channels: int

    # user-friendly поля
    name: Optional[str] = None
    location: Optional[str] = None

    # версия прошивки как число
    firmware_version: Optional[str] = None

    is_active: bool

    # динамические поля
    status: Literal["online", "offline"] = "offline"
    last_seen: Optional[int] = None

    # вложенные каналы
    channels: Optional[List[ChannelSchema]] = None

    model_config = ConfigDict(from_attributes=True)

class DeviceUpdateSchema(BaseModel):
    name: Optional[str] = None
    location: Optional[str] = None
    firmware_version: Optional[str] = None
    is_active: Optional[bool] = None
    num_channels: Optional[int] = None
    type: Optional[str] = None