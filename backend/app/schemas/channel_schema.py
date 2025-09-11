from pydantic import BaseModel, ConfigDict
from typing import Optional


class ChannelSchema(BaseModel):
    id: int
    device_id: int
    index: int
    type: str
    name: Optional[str] = None

    model_config = ConfigDict(from_attributes=True, extra="ignore")


class ChannelCreateSchema(BaseModel):
    index: int
    type: str
    name: Optional[str] = None


class ChannelUpdateSchema(BaseModel):
    type: Optional[str] = None
    name: Optional[str] = None
