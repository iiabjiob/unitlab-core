from pydantic import BaseModel, ConfigDict
from typing import Optional, Literal

class DeviceSchema(BaseModel):
    unit_id: str
    type: str
    channels: int

    # user-friendly поля
    name: Optional[str] = None
    location: Optional[str] = None

    # версия прошивки как число
    firmware_version: Optional[float] = None

    is_active: bool

    # динамические поля
    status: Literal["online", "offline"] = "offline"
    last_seen: Optional[int] = None

    model_config = ConfigDict(from_attributes=True)

class DeviceUpdateSchema(BaseModel):
    name: Optional[str] = None
    location: Optional[str] = None
    firmware_version: Optional[float] = None
    is_active: Optional[bool] = None
    channels: Optional[int] = None
    type: Optional[str] = None