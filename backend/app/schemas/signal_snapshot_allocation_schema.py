from datetime import datetime

from pydantic import BaseModel, ConfigDict, Field


class AllocationMappingMetaSchema(BaseModel):
    sheet_index: int | None = None
    sheet_name: str | None = None
    column_key: str | None = None


class AllocationMappingItemSchema(BaseModel):
    channel_id: str
    signal_key: str
    signal_row_index: int
    meta: AllocationMappingMetaSchema | None = None


class SignalSnapshotAllocationSchema(BaseModel):
    id: int
    workspace_id: int
    signal_snapshot_id: int
    mapping: list[AllocationMappingItemSchema]
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)


class SignalSnapshotAllocationUpdateSchema(BaseModel):
    mapping: list[AllocationMappingItemSchema] = Field(default_factory=list)
