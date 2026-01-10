from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict

from app.models.signal_snapshot import SignalSnapshotStatus


class SignalSnapshotSummarySchema(BaseModel):
    id: int
    workspace_id: int
    status: SignalSnapshotStatus
    source_filename: str | None
    source_hash: str | None
    rows_count: int
    schema_version: int
    locked_at: datetime | None
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)


class SignalSnapshotDetailSchema(SignalSnapshotSummarySchema):
    data: list[dict[str, Any]]
