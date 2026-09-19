from typing import ClassVar

# app/schemas/switchgear_schema.py
from pydantic import BaseModel, ConfigDict, Field


class SwitchgearBindingSchema(BaseModel):
    id: int
    channel_id: int | None
    role: str
    delay_ms: int

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True, extra="ignore")


class SwitchgearBindingCreateSchema(BaseModel):
    channel_id: int | None = None
    role: str
    delay_ms: int = 0


class SwitchgearSchema(BaseModel):
    id: int
    name: str
    switchgear_type: str
    workspace_ids: list[int] = Field(default_factory=list)
    bindings: list[SwitchgearBindingSchema] = Field(default_factory=list)

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True, extra="ignore")


class SwitchgearCreateSchema(BaseModel):
    name: str
    switchgear_type: str = "switchgear"
    bindings: list[SwitchgearBindingCreateSchema] = Field(default_factory=list)


class SwitchgearUpdateSchema(BaseModel):
    name: str | None = None
    switchgear_type: str | None = None
    bindings: list[SwitchgearBindingCreateSchema] | None = None
