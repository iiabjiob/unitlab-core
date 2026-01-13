from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field

from app.models.test_run import TestRunStatus
from app.schemas.allocation_schema import AllocationCreateSchema, AllocationSchema
from app.schemas.test_run_signal_schema import TestRunSignalSnapshotSummarySchema


class TestRunCreateSchema(BaseModel):
    workspace_id: int
    sequence_ids: list[int] = Field(min_length=1)
    allocation: AllocationCreateSchema = Field(default_factory=AllocationCreateSchema)
    allow_empty_allocation: bool = False


class TestRunSchema(BaseModel):
    id: int
    workspace_id: int
    allocation: AllocationSchema | None
    sequence_ids: list[int]
    status: TestRunStatus
    created_at: datetime
    started_at: datetime | None
    finished_at: datetime | None
    execution_meta: dict[str, Any] | None
    snapshot: TestRunSignalSnapshotSummarySchema | None

    model_config = ConfigDict(from_attributes=True)
