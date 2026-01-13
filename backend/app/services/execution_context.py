"""Runtime execution context built from a TestRun."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Optional

from app.models.sequence import SequenceStepType
from app.models.test_run import TestRunStatus


@dataclass(frozen=True)
class AllocationEntryDTO:
    channel_id: int
    signal_id: Optional[int]
    signal_key: Optional[str]
    signal_metadata: Optional[dict[str, Any]]


@dataclass(frozen=True)
class SequenceStepDTO:
    id: int
    order_index: int
    sequence_step_type: SequenceStepType
    channel_id: Optional[int]
    payload: Optional[dict[str, Any]]


@dataclass(frozen=True)
class SequenceDTO:
    id: int
    steps: tuple[SequenceStepDTO, ...]


@dataclass(frozen=True)
class ExecutionContext:
    """Immutable runtime context for executing a TestRun."""

    test_run_id: int
    status: TestRunStatus
    allocation_entries: tuple[AllocationEntryDTO, ...]
    sequences: tuple[SequenceDTO, ...]
