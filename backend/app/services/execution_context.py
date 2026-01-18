"""Runtime execution context derived from stored sequences."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Optional

from app.models.sequence import SequenceStepType


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