from pydantic import BaseModel

class DeviceOut(BaseModel):
    unit_id: str
    type: str
    is_active: bool

    class Config:
        orm_mode = True