"""Runtime execution context derived from stored sequences."""
from __future__ import annotations

from dataclasses import dataclass

from app.models.sequence import SequenceStepType


@dataclass(frozen=True)
class SequenceStepDTO:
    id: int
    order_index: int
    sequence_step_type: SequenceStepType
    channel_id: int | None
    payload: dict[str, object] | None


@dataclass(frozen=True)
class SequenceDTO:
    id: int
    steps: tuple[SequenceStepDTO, ...]
