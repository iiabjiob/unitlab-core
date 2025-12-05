from __future__ import annotations

from enum import Enum


class SequenceStepType(str, Enum):
    WAIT = "WAIT"
    DO_LATCH = "DO_LATCH"
    DO_PULSE = "DO_PULSE"
    DO_PAIR = "DO_PAIR"
    DO_BITMASK = "DO_BITMASK"
    AO_SET = "AO_SET"
