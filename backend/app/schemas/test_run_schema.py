from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field, ValidationInfo, field_validator

from app.models.test_run import TestRunMode, TestRunStatus
from app.schemas.allocation_schema import AllocationCreateSchema, AllocationSchema


class TestRunCreateSchema(BaseModel):
    workspace_id: int
    sequence_ids: list[int] = Field(min_length=1)
    allocation: AllocationCreateSchema = Field(default_factory=AllocationCreateSchema)
    allow_empty_allocation: bool = False
    mode: TestRunMode = TestRunMode.CHANNEL
    signal_snapshot_id: int | None = None

    @field_validator("signal_snapshot_id")
    @classmethod
    def validate_signal_snapshot(cls, value: int | None, info: ValidationInfo) -> int | None:
        mode = info.data.get("mode", TestRunMode.CHANNEL)
        if mode == TestRunMode.SIGNAL and value is None:
            raise ValueError("signal_snapshot_id is required for signal mode test runs")
        if mode == TestRunMode.CHANNEL and value is not None:
            raise ValueError("signal_snapshot_id must be omitted in channel mode test runs")
        return value


class TestRunSchema(BaseModel):
    id: int
    workspace_id: int
    signal_snapshot_id: int | None
    allocation: AllocationSchema | None
    sequence_ids: list[int]
    mode: TestRunMode
    status: TestRunStatus
    created_at: datetime
    started_at: datetime | None
    finished_at: datetime | None
    execution_meta: dict[str, Any] | None

    model_config = ConfigDict(from_attributes=True)
