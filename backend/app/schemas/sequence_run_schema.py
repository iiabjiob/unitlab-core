"""Pydantic schemas describing sequence runtimes."""
from __future__ import annotations

from datetime import datetime
from typing import List, Literal, Optional

from pydantic import BaseModel, ConfigDict, Field

SequenceRunStatusLiteral = Literal["running", "completed", "stopped", "error"]
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


class SequenceStateSchema(BaseModel):
    sequence_id: int
    status: Literal["idle", "running", "completed", "stopped", "error"]
    run_id: Optional[int] = None
    current_step_index: int
    total_steps: int
    completed_step_ids: List[int]
    last_error: Optional[str] = None
    started_at: Optional[datetime] = None
    finished_at: Optional[datetime] = None