from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class AllocationEntryBaseSchema(BaseModel):
    channel_id: int
    signal_key: str | None = None
    signal_metadata: dict[str, Any] | None = None


class AllocationEntrySchema(AllocationEntryBaseSchema):
    id: int

    model_config = ConfigDict(from_attributes=True)


class AllocationSchema(BaseModel):
    id: int
    test_run_id: int
    notes: str | None
    created_at: datetime
    updated_at: datetime
    entries: list[AllocationEntrySchema]

    model_config = ConfigDict(from_attributes=True)


class AllocationEntryCreateSchema(AllocationEntryBaseSchema):
    pass


class AllocationCreateSchema(BaseModel):
    notes: str | None = None
    entries: list[AllocationEntryCreateSchema] = Field(default_factory=list)
