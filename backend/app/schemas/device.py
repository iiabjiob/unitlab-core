from pydantic import BaseModel, ConfigDict

class DeviceOut(BaseModel):
    unit_id: str
    type: str
    is_active: bool

    model_config = ConfigDict(from_attributes=True)