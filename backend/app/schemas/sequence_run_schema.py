"""Pydantic schemas describing sequence runtimes."""
from __future__ import annotations

from datetime import datetime
from typing import List, Literal, Optional

from pydantic import BaseModel, ConfigDict, Field

SequenceRunStatusLiteral = Literal["pending", "running", "cancelling", "completed", "stopped", "error"]
SequenceRunStepStatusLiteral = Literal[
    "pending",
    "running",
    "completed",
    "error",
    "cancelled",
]


class SequenceRunStepSchema(BaseModel):
    id: int
    run_id: int
    sequence_step_id: int
    order_index: int
    status: SequenceRunStepStatusLiteral
    started_at: Optional[datetime] = None
    finished_at: Optional[datetime] = None
    error_message: Optional[str] = None
    elapsed_ms: Optional[int] = None

    model_config = ConfigDict(from_attributes=True)


class SequenceRunSchema(BaseModel):
    id: int
    sequence_id: int
    status: SequenceRunStatusLiteral
    started_at: datetime
    finished_at: Optional[datetime] = None
    error_message: Optional[str] = None
    current_step_index: int
    steps: List[SequenceRunStepSchema] = Field(default_factory=list)

    model_config = ConfigDict(from_attributes=True)


class SequenceRuntimeSchema(BaseModel):
    execution_path: List[str] = Field(default_factory=list)
    active_sequence_id: Optional[int] = None
    active_sequence_name: Optional[str] = None
    active_step_id: Optional[int] = None
    active_step_index: Optional[int] = None
    active_total_steps: Optional[int] = None
    active_step_type: Optional[str] = None
    iteration_current: Optional[int] = None
    iteration_total: Optional[int] = None
    repeat_mode: Optional[Literal["times", "duration", "until_stopped"]] = None
    run_elapsed_ms: Optional[int] = None


class SequenceStateSchema(BaseModel):
    sequence_id: int
    status: Literal["idle", "pending", "running", "cancelling", "completed", "stopped", "error"]
    run_id: Optional[int] = None
    current_step_index: int
    total_steps: int
    completed_step_ids: List[int]
    last_error: Optional[str] = None
    started_at: Optional[datetime] = None
    finished_at: Optional[datetime] = None
    runtime: Optional[SequenceRuntimeSchema] = None
