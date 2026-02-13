from __future__ import annotations

from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class TestRunSignalSnapshotSummarySchema(BaseModel):
    test_run_id: int
    workspace_id: int
    captured_at: datetime

    model_config = ConfigDict(from_attributes=True)


class TestRunSignalSnapshotEntrySchema(BaseModel):
    id: int
    snapshot_id: int
    live_signal_id: int | None = None
    signal_key: str
    name: str
    io_direction: str
    allocation_channel_id: int | None = None
    allocation_metadata: dict[str, Any] | None = None
    entry_metadata: dict[str, Any] = Field(default_factory=dict)
    created_at: datetime

    model_config = ConfigDict(from_attributes=True)


class TestRunSignalSnapshotSchema(TestRunSignalSnapshotSummarySchema):
    entries: list[TestRunSignalSnapshotEntrySchema] = Field(default_factory=list)


class AllocationEntrySchema(BaseModel):
    id: int
    channel_id: int
    signal_id: int | None = None
    signal_key: str | None = None
    signal_metadata: dict[str, Any] | None = None


class AllocationSchema(BaseModel):
    id: int
    test_run_id: int
    notes: str | None = None
    created_at: datetime
    updated_at: datetime
    entries: list[AllocationEntrySchema] = Field(default_factory=list)


class TestRunRecordSchema(BaseModel):
    id: int
    workspace_id: int
    source_test_run_id: int | None = None
    allocation_revision: int = 1
    allocation: AllocationSchema | None = None
    sequence_ids: list[int] = Field(default_factory=list)
    status: str
    created_at: datetime
    started_at: datetime | None = None
    finished_at: datetime | None = None
    execution_meta: dict[str, Any] | None = None
    snapshot: TestRunSignalSnapshotSummarySchema | None = None


class AllocationEntryInputSchema(BaseModel):
    channel_id: int
    signal_id: int | None = None
    signal_metadata: dict[str, Any] | None = None


class AllocationCreateSchema(BaseModel):
    notes: str | None = None
    entries: list[AllocationEntryInputSchema] = Field(default_factory=list)


class TestRunCreateSchema(BaseModel):
    workspace_id: int
    sequence_ids: list[int] = Field(default_factory=list)
    allocation: AllocationCreateSchema | None = None
    allow_empty_allocation: bool = False


class TestRunReallocationItemSchema(BaseModel):
    allocation_entry_id: int
    channel_id: int


class TestRunReallocateSchema(BaseModel):
    notes: str | None = None
    reallocation: list[TestRunReallocationItemSchema] = Field(default_factory=list)


class TestRunPreflightUnitSchema(BaseModel):
    device_id: int
    unit_id: str
    available: bool
    last_seen_at: datetime | None = None
    reason: str | None = None


class TestRunPreflightEntrySchema(BaseModel):
    allocation_entry_id: int
    channel_id: int
    signal_id: int | None = None
    signal_key: str | None = None
    required_direction: str | None = None
    available: bool
    reason: str | None = None
    recommended_channel_ids: list[int] = Field(default_factory=list)


class TestRunPreflightSchema(BaseModel):
    test_run_id: int
    ready: bool
    reallocation_required: bool
    units: list[TestRunPreflightUnitSchema] = Field(default_factory=list)
    entries: list[TestRunPreflightEntrySchema] = Field(default_factory=list)
