from __future__ import annotations

import asyncio
from types import SimpleNamespace

import pytest

from app.services.domain_errors import SequenceNotApplicableError
from app.services.sequence_executor import SequenceCancellationRequested
from app.services.sequence_runner import (
    SequenceRunner,
    _sequence_readback_targets,
    _wait_for_sequence_readback,
)


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


def test_sequence_readback_targets_cover_pair_and_device_bitmask() -> None:
    pair_targets, pair_analog = _sequence_readback_targets(
        action="do_pair",
        payload={"channel_indexes": [4, 7], "state2b": 2},
    )
    all_targets, all_analog = _sequence_readback_targets(
        action="do_all",
        payload={"channel_indexes": [1, 5], "bitmask": 0b10},
    )

    assert pair_targets == [(4, 0), (7, 1)]
    assert pair_analog is False
    assert all_targets == [(1, 1), (5, 0)]
    assert all_analog is False


def test_sequence_bitmask_readback_rejects_missing_channel_indexes() -> None:
    with pytest.raises(SequenceNotApplicableError, match="resolved channel indexes"):
        _sequence_readback_targets(
            action="do_all",
            payload={"bitmask": 1, "channel_indexes": []},
        )


def test_sequence_readback_requires_matching_fresh_packet() -> None:
    class Redis:
        async def get(self, key: str):
            if key.endswith(":last_state_packet_id"):
                return "42"
            if key.endswith(":bitmask"):
                return str(1 << 2)
            return None

    assert asyncio.run(
        _wait_for_sequence_readback(
            Redis(),
            unit_id="DO-001",
            targets=[(2, 1)],
            analog=False,
            packet_id=42,
            timeout_ms=100,
        )
    ) is True


def test_sequence_readback_stops_immediately_when_cancelled() -> None:
    class Redis:
        async def get(self, key: str):
            return "0"

    cancel_event = asyncio.Event()
    cancel_event.set()

    with pytest.raises(SequenceCancellationRequested):
        asyncio.run(
            _wait_for_sequence_readback(
                Redis(),
                unit_id="DO-001",
                targets=[(2, 1)],
                analog=False,
                packet_id=42,
                timeout_ms=3000,
                cancel_event=cancel_event,
            )
        )
