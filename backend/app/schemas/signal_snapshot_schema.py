from __future__ import annotations

from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class SnapshotSheetSchema(BaseModel):
    name: str
    index: int
    headers: list[str] = Field(default_factory=list)
    rows_count: int
    rows: list[dict[str, Any]] = Field(default_factory=list)


class SignalSnapshotDataSchema(BaseModel):
    version: int = 2
    sheet_count: int
    default_sheet_index: int
    sheets: list[SnapshotSheetSchema] = Field(default_factory=list)


class SignalSnapshotSummarySchema(BaseModel):
    id: int
    workspace_id: int
    status: str
    source_filename: str | None = None
    source_hash: str | None = None
    rows_count: int
    schema_version: int
    locked_at: datetime | None = None
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)


class SignalSnapshotSchema(SignalSnapshotSummarySchema):
    data: SignalSnapshotDataSchema | list[dict[str, Any]]


class AllocationMappingMetaSchema(BaseModel):
    sheet_index: int | None = None
    sheet_name: str | None = None
    column_key: str | None = None


class AllocationMappingItemSchema(BaseModel):
    channel_id: str
    signal_key: str
    signal_row_index: int = 0
    meta: AllocationMappingMetaSchema | None = None


class SignalSnapshotAllocationSchema(BaseModel):
    id: int
    workspace_id: int
    signal_snapshot_id: int
    mapping: list[AllocationMappingItemSchema] = Field(default_factory=list)
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)


class SignalSnapshotAllocationUpdateSchema(BaseModel):
    mapping: list[AllocationMappingItemSchema] = Field(default_factory=list)


class SignalImportMetaSchema(BaseModel):
    sheet_name: str | None = None
    source_sheet_name: str | None = None
    hmi_representation: str | None = None
    type_column: str | None = None
    type_mapping: dict[str, str] = Field(default_factory=dict)
    internal_type_column: str | None = None
