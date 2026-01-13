from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field

from app.models.signal import SignalIODirection


class SignalBaseSchema(BaseModel):
    key: str = Field(min_length=1)
    name: str = Field(min_length=1)
    io_direction: SignalIODirection
    category: str | None = None
    metadata: dict[str, Any] = Field(default_factory=dict)
    is_active: bool = True


class SignalCreateSchema(SignalBaseSchema):
    pass


class SignalUpdateSchema(BaseModel):
    name: str | None = None
    io_direction: SignalIODirection | None = None
    category: str | None = None
    metadata: dict[str, Any] | None = None
    is_active: bool | None = None


class SignalSchema(SignalBaseSchema):
    metadata: dict[str, Any] = Field(default_factory=dict, alias="signal_metadata")
    id: int
    workspace_id: int
    deleted_at: datetime | None
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True, populate_by_name=True)
