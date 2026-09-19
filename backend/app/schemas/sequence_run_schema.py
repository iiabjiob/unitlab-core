"""Pydantic schemas describing sequence runtimes."""
from __future__ import annotations

from datetime import datetime
from typing import ClassVar, Literal

from pydantic import BaseModel, ConfigDict, Field

SequenceRunStatusLiteral = Literal["pending", "running", "cancelling", "completed", "completed_with_issues", "stopped", "error"]
SequenceRunStepStatusLiteral = Literal[
    "pending",
    "running",
    "completed",
    "error",
    "blocked",
    "cancelled",
]


class SequenceRunStepSchema(BaseModel):
    id: int
    run_id: int
    sequence_step_id: int
    order_index: int
    status: SequenceRunStepStatusLiteral
    started_at: datetime | None = None
    finished_at: datetime | None = None
    error_message: str | None = None
    elapsed_ms: int | None = None

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True)


class SequenceRunSchema(BaseModel):
    id: int
    sequence_id: int
    status: SequenceRunStatusLiteral
    started_at: datetime
    finished_at: datetime | None = None
    error_message: str | None = None
    current_step_index: int
    steps: list[SequenceRunStepSchema] = Field(default_factory=list)

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True)


class SequenceRuntimeSchema(BaseModel):
    execution_path: list[str] = Field(default_factory=list)
    active_sequence_id: int | None = None
    active_sequence_name: str | None = None
    active_step_id: int | None = None
    active_step_index: int | None = None
    active_total_steps: int | None = None
    active_step_type: str | None = None
    iteration_current: int | None = None
    iteration_total: int | None = None
    repeat_mode: Literal["times", "duration", "until_stopped"] | None = None
    run_elapsed_ms: int | None = None


class SequenceStateSchema(BaseModel):
    sequence_id: int
    status: Literal["idle", "pending", "running", "cancelling", "completed", "completed_with_issues", "stopped", "error"]
    run_id: int | None = None
    current_step_index: int
    total_steps: int
    completed_step_ids: list[int]
    blocked_step_ids: list[int] = Field(default_factory=list)
    last_error: str | None = None
    started_at: datetime | None = None
    finished_at: datetime | None = None
    runtime: SequenceRuntimeSchema | None = None
