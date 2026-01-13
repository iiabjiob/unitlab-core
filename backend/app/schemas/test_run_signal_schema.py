from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field

from app.models.signal import SignalIODirection


class TestRunSignalSnapshotEntrySchema(BaseModel):
    id: int
    snapshot_id: int
    live_signal_id: int | None
    signal_key: str
    name: str
    io_direction: SignalIODirection
    allocation_channel_id: int | None
    allocation_metadata: dict[str, Any] | None
    metadata: dict[str, Any] = Field(alias="entry_metadata")
    created_at: datetime

    model_config = ConfigDict(from_attributes=True, populate_by_name=True)


class TestRunSignalSnapshotSummarySchema(BaseModel):
    test_run_id: int
    workspace_id: int
    captured_at: datetime

    model_config = ConfigDict(from_attributes=True)


class TestRunSignalSnapshotSchema(TestRunSignalSnapshotSummarySchema):
    entries: list[TestRunSignalSnapshotEntrySchema]
