# app/schemas/switchgear_schema.py
from pydantic import BaseModel, ConfigDict, Field
from typing import Optional


class SwitchgearBindingSchema(BaseModel):
    id: int
    channel_id: Optional[int]
    role: str
    delay_ms: int

    model_config = ConfigDict(from_attributes=True, extra="ignore")


class SwitchgearBindingCreateSchema(BaseModel):
    channel_id: Optional[int] = None
    role: str
    delay_ms: int = 0


class SwitchgearSchema(BaseModel):
    id: int
    name: str
    switchgear_type: str
    bindings: list[SwitchgearBindingSchema] = Field(default_factory=list)

    model_config = ConfigDict(from_attributes=True, extra="ignore")


class SwitchgearCreateSchema(BaseModel):
    name: str
    switchgear_type: str = "switchgear"
    bindings: list[SwitchgearBindingCreateSchema] = Field(default_factory=list)


class SwitchgearUpdateSchema(BaseModel):
    name: Optional[str] = None
    switchgear_type: Optional[str] = None
    bindings: Optional[list[SwitchgearBindingCreateSchema]] = None
