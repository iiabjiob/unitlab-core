from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict

from app.models.test_run import TestRunStatus


class TestRunCreateSchema(BaseModel):
    sequence_id: int
    signal_snapshot_id: int


class TestRunSchema(BaseModel):
    id: int
    workspace_id: int
    sequence_id: int
    signal_snapshot_id: int
    allocation_snapshot: list[dict[str, Any]]
    status: TestRunStatus
    created_at: datetime
    started_at: datetime | None
    finished_at: datetime | None
    execution_meta: dict[str, Any] | None

    model_config = ConfigDict(from_attributes=True)
