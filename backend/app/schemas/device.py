from pydantic import BaseModel, ConfigDict
from typing import Optional

class DeviceOut(BaseModel):
    unit_id: str
    type: str
    channels: int
    location: Optional[str] = None
    firmware_version: Optional[str] = None
    is_active: bool

    # динамика
    status: Optional[str] = None
    last_seen: Optional[int] = None

    model_config = ConfigDict(from_attributes=True)
