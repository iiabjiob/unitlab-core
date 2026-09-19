from __future__ import annotations

from types import SimpleNamespace
from typing import cast

import pytest

from app.services.domain_errors import SequenceNotApplicableError
from app.services.sequence_runner import SequenceRunner
from app.models.sequence.sequence_step import SequenceStep


class _StepType:
    def __init__(self, value: str):
        self.value: str = value


def _step(step_type: str, channel_id: int | None = None) -> SequenceStep:
    return cast(
        SequenceStep,
        cast(object, SimpleNamespace(sequence_step_type=_StepType(step_type), channel_id=channel_id)),
    )


def test_resolve_target_sequence_id_for_call_step() -> None:
    target = SequenceRunner._resolve_target_sequence_id(  # pyright: ignore[reportPrivateUsage]
        step=_step("CALL_SEQUENCE"),
        payload={"target_sequence_id": "42"},
    )

    assert target == 42


def test_resolve_target_sequence_id_requires_positive_integer() -> None:
    with pytest.raises(SequenceNotApplicableError, match="positive integer"):
        _ = SequenceRunner._resolve_target_sequence_id(  # pyright: ignore[reportPrivateUsage]
            step=_step("REPEAT_SEQUENCE"),
            payload={"target_sequence_id": "bad"},
        )


def test_repeat_config_supports_times_duration_and_until_stopped() -> None:
    fixed = SequenceRunner._parse_repeat_config({"repeat_mode": "times", "iterations": 5})  # pyright: ignore[reportPrivateUsage]
    timed = SequenceRunner._parse_repeat_config({"repeat_mode": "duration", "duration_ms": 1500})  # pyright: ignore[reportPrivateUsage]
    endless = SequenceRunner._parse_repeat_config({"repeat_mode": "until_stopped"})  # pyright: ignore[reportPrivateUsage]

    assert fixed.mode == "times"
    assert fixed.iterations == 5
    assert timed.mode == "duration"
    assert timed.duration_ms == 1500
    assert endless.mode == "until_stopped"


def test_repeat_config_rejects_non_positive_iterations() -> None:
    with pytest.raises(SequenceNotApplicableError, match="iterations"):
        _ = SequenceRunner._parse_repeat_config({"repeat_mode": "times", "iterations": 0})  # pyright: ignore[reportPrivateUsage]


def test_repeat_config_rejects_non_positive_duration() -> None:
    with pytest.raises(SequenceNotApplicableError, match="duration_ms"):
        _ = SequenceRunner._parse_repeat_config({"repeat_mode": "duration", "duration_ms": 0})  # pyright: ignore[reportPrivateUsage]


def test_call_step_does_not_require_primary_signal_binding() -> None:
    result = SequenceRunner._resolve_primary_channel_id(  # pyright: ignore[reportPrivateUsage]
        step=_step("CALL_SEQUENCE"),
        payload={"signal_key": "child_sequence"},
        signal_bindings={},
    )

    assert result is None
