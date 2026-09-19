from __future__ import annotations

from datetime import datetime
from typing import ClassVar

from pydantic import BaseModel, ConfigDict, Field


class SignalBaseSchema(BaseModel):
    key: str
    name: str
    io_direction: str
    category: str | None = None
    signal_metadata: dict[str, object] = Field(default_factory=dict)
    is_active: bool = True


class SignalCreateSchema(BaseModel):
    key: str
    name: str
    io_direction: str
    category: str | None = None
    metadata: dict[str, object] = Field(default_factory=dict)
    is_active: bool = True


class SignalUpdateSchema(BaseModel):
    name: str | None = None
    io_direction: str | None = None
    category: str | None = None
    metadata: dict[str, object] | None = None
    is_active: bool | None = None


class SignalBulkDeleteRequestSchema(BaseModel):
    signal_ids: list[int] = Field(default_factory=list)


class SignalBulkDeleteResponseSchema(BaseModel):
    requested_count: int
    deleted_count: int


class SignalSchema(SignalBaseSchema):
    id: int
    workspace_id: int
    deleted_at: datetime | None = None
    created_at: datetime
    updated_at: datetime

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True)
