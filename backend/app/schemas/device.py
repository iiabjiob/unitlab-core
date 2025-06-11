from pydantic import BaseModel, ConfigDict

class DeviceOut(BaseModel):
    unit_id: str
    type: str
    is_active: bool
    channels: int
    location: str
    firmware_version: str

    model_config = ConfigDict(from_attributes=True)