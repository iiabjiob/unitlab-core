from __future__ import annotations

from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field

from app.schemas.signal_import_schema import SignalImportMetaSchema


class SignalSheetSchema(BaseModel):
    id: int
    workspace_id: int
    source_filename: str | None = None
    source_hash: str | None = None
    rows_count: int = 0
    schema_version: int = 2
    data: dict[str, Any] = Field(default_factory=dict)
    import_meta: SignalImportMetaSchema | None = None
    signals_count: int = 0
    allocated_count: int = 0
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)


class SignalSheetPresetSchema(BaseModel):
    id: int
    workspace_id: int
    name: str
    import_meta: SignalImportMetaSchema
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)


class SignalSheetPresetCreateSchema(BaseModel):
    name: str = Field(min_length=1, max_length=120)
    import_meta: SignalImportMetaSchema


class SignalSheetImportResponseSchema(BaseModel):
    sheet: SignalSheetSchema


class SignalSheetImportPreviewSheetSchema(BaseModel):
    name: str
    index: int
    headers: list[str] = Field(default_factory=list)
    rows_count: int = 0
    rows: list[dict[str, Any]] = Field(default_factory=list)


class SignalSheetImportPreviewResponseSchema(BaseModel):
    rows_count: int = 0
    sheet_count: int = 0
    default_sheet_index: int = 0
    sheets: list[SignalSheetImportPreviewSheetSchema] = Field(default_factory=list)


class SignalAllocationRowSchema(BaseModel):
    signal_id: int
    signal_key: str
    signal_name: str
    signal_direction: str
    signal_category: str | None = None
    signal_metadata: dict[str, Any] = Field(default_factory=dict)
    channel_id: int | None = None
    channel_type: str | None = None
    channel_index: int | None = None
    channel_label: str | None = None
    device_id: int | None = None
    unit_id: str | None = None
    unit_online: bool | None = None
    unit_last_seen_at: datetime | None = None
    tested_at: datetime | None = None


class SignalAllocationUpdateItemSchema(BaseModel):
    signal_id: int
    channel_id: int | None = None
    allocation_meta: dict[str, Any] | None = None


class SignalAllocationBulkUpdateSchema(BaseModel):
    entries: list[SignalAllocationUpdateItemSchema] = Field(default_factory=list)


class SignalAutoAllocateSchema(BaseModel):
    signal_ids: list[int] = Field(default_factory=list)
    prefer_online: bool = True
    prefer_single_unit: bool = False
    overwrite_existing: bool = False


class SignalAllocationEnsureSchema(BaseModel):
    signal_ids: list[int] = Field(default_factory=list)
    prefer_online: bool = True


class SignalAllocationMarkTestedSchema(BaseModel):
    signal_ids: list[int] = Field(default_factory=list)


class SignalTestRunJobSchema(BaseModel):
    signal_ids: list[int] = Field(default_factory=list)
    signal_interval_ms: int = Field(default=1000, ge=100, le=10000)
    toggle_mode: str = Field(default="single", pattern="^(single|double)$")


class SignalAutoAllocateResultSchema(BaseModel):
    assigned: int
    skipped: int
    missing: int
    unassigned_signal_ids: list[int] = Field(default_factory=list)


class SignalAutoAllocateResponseSchema(BaseModel):
    result: SignalAutoAllocateResultSchema
    rows: list[SignalAllocationRowSchema] = Field(default_factory=list)


class SignalAllocationEnsureResponseSchema(BaseModel):
    result: SignalAutoAllocateResultSchema
    rows: list[SignalAllocationRowSchema] = Field(default_factory=list)


class SignalJobStatusSchema(BaseModel):
    job_id: str
    workspace_id: int
    operation: str
    status: str
    progress_total: int = 0
    progress_done: int = 0
    message: str | None = None
    error: str | None = None
    result: dict[str, Any] = Field(default_factory=dict)
    created_at: datetime
    updated_at: datetime


SignalAllocationJobStatusSchema = SignalJobStatusSchema


class SignalJobControlSchema(BaseModel):
    action: str = Field(pattern="^(pause|resume|stop)$")


SignalAllocationJobControlSchema = SignalJobControlSchema
