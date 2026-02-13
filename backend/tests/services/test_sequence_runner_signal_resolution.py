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


def test_resolve_primary_channel_uses_signal_binding() -> None:
    result = SequenceRunner._resolve_primary_channel_id(
        step=_step("DO_LATCH"),
        payload={"signal_key": "do_primary"},
        signal_bindings={"do_primary": 33},
    )

    assert result == 33


def test_resolve_primary_channel_requires_mapping_for_do_steps() -> None:
    with pytest.raises(SequenceNotApplicableError):
        SequenceRunner._resolve_primary_channel_id(
            step=_step("DO_LATCH"),
            payload={"signal_key": "do_primary"},
            signal_bindings={},
        )


def test_wait_step_can_keep_unmapped_signal_key() -> None:
    result = SequenceRunner._resolve_primary_channel_id(
        step=_step("WAIT"),
        payload={"signal_key": "di_feedback"},
        signal_bindings={},
    )

    assert result is None


def test_resolve_payload_channel_ids_from_signal_keys() -> None:
    payload = SequenceRunner._resolve_payload_channel_ids(
        {"signal_keys": ["left", "right"]},
        signal_bindings={"left": 1, "right": 2},
    )

    assert payload["channel_ids"] == [1, 2]


def test_resolve_payload_channel_ids_raises_for_missing_signal_key() -> None:
    with pytest.raises(SequenceNotApplicableError):
        SequenceRunner._resolve_payload_channel_ids(
            {"signal_keys": ["left", "right"]},
            signal_bindings={"left": 1},
        )
