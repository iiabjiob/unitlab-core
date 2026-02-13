from __future__ import annotations

from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class SignalBaseSchema(BaseModel):
    key: str
    name: str
    io_direction: str
    category: str | None = None
    signal_metadata: dict[str, Any] = Field(default_factory=dict)
    is_active: bool = True


class SignalCreateSchema(BaseModel):
    key: str
    name: str
    io_direction: str
    category: str | None = None
    metadata: dict[str, Any] = Field(default_factory=dict)
    is_active: bool = True


class SignalUpdateSchema(BaseModel):
    name: str | None = None
    io_direction: str | None = None
    category: str | None = None
    metadata: dict[str, Any] | None = None
    is_active: bool | None = None


class SignalSchema(SignalBaseSchema):
    id: int
    workspace_id: int
    deleted_at: datetime | None = None
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)
