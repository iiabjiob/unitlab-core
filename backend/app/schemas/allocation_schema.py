from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class AllocationMappingItemSchema(BaseModel):
    channel_id: str
    signal_key: str
    signal_row_index: int
    meta: dict[str, Any] | None = None


class AllocationSchema(BaseModel):
    id: int
    workspace_id: int
    signal_snapshot_id: int
    mapping: list[AllocationMappingItemSchema]
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)


class AllocationUpdateSchema(BaseModel):
    mapping: list[AllocationMappingItemSchema] = Field(default_factory=list)
