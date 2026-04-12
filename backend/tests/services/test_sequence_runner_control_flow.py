from __future__ import annotations

from types import SimpleNamespace

import pytest

from app.services.domain_errors import SequenceNotApplicableError
from app.services.sequence_runner import SequenceRunner


class _StepType:
    def __init__(self, value: str):
        self.value = value


def _step(step_type: str, channel_id: int | None = None):
    return SimpleNamespace(sequence_step_type=_StepType(step_type), channel_id=channel_id)


def test_resolve_target_sequence_id_for_call_step() -> None:
    target = SequenceRunner._resolve_target_sequence_id(
        step=_step("CALL_SEQUENCE"),
        payload={"target_sequence_id": "42"},
    )

    assert target == 42


def test_resolve_target_sequence_id_requires_positive_integer() -> None:
    with pytest.raises(SequenceNotApplicableError, match="positive integer"):
        SequenceRunner._resolve_target_sequence_id(
            step=_step("REPEAT_SEQUENCE"),
            payload={"target_sequence_id": "bad"},
        )


def test_repeat_config_supports_times_duration_and_until_stopped() -> None:
    fixed = SequenceRunner._parse_repeat_config({"repeat_mode": "times", "iterations": 5})
    timed = SequenceRunner._parse_repeat_config({"repeat_mode": "duration", "duration_ms": 1500})
    endless = SequenceRunner._parse_repeat_config({"repeat_mode": "until_stopped"})

    assert fixed.mode == "times"
    assert fixed.iterations == 5
    assert timed.mode == "duration"
    assert timed.duration_ms == 1500
    assert endless.mode == "until_stopped"


def test_repeat_config_rejects_non_positive_iterations() -> None:
    with pytest.raises(SequenceNotApplicableError, match="iterations"):
        SequenceRunner._parse_repeat_config({"repeat_mode": "times", "iterations": 0})


def test_repeat_config_rejects_non_positive_duration() -> None:
    with pytest.raises(SequenceNotApplicableError, match="duration_ms"):
        SequenceRunner._parse_repeat_config({"repeat_mode": "duration", "duration_ms": 0})


def test_call_step_does_not_require_primary_signal_binding() -> None:
    result = SequenceRunner._resolve_primary_channel_id(
        step=_step("CALL_SEQUENCE"),
        payload={"signal_key": "child_sequence"},
        signal_bindings={},
    )

    assert result is None
