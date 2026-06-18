from __future__ import annotations

import asyncio

import pytest

from app.infrastructure.protocol.modes import Cmd
from app.models.sequence.types import SequenceStepType
from app.services.domain_errors import SequenceNotApplicableError
from app.services.sequence_executor import ChannelInfo, SequenceExecutor, StepContext


def _pair_context(
    *,
    state2b: object = 0,
    first: ChannelInfo | None = None,
    second: ChannelInfo | None = None,
) -> StepContext:
    ch_a = first or ChannelInfo(id=10, device_id=1, unit_id="DO-001", channel_index=2)
    ch_b = second or ChannelInfo(id=11, device_id=1, unit_id="DO-001", channel_index=3)
    return StepContext(
        index=0,
        sequence_step_id=1,
        step_type=SequenceStepType.DO_PAIR,
        payload={"state2b": state2b},
        primary_channel=None,
        pair_channels=[ch_a, ch_b],
        target_device=None,
    )


async def _noop_probe() -> None:
    return None


def test_do_pair_rejects_duplicate_channel_ids() -> None:
    executor = SequenceExecutor()
    channel = ChannelInfo(id=10, device_id=1, unit_id="DO-001", channel_index=2)
    ctx = _pair_context(first=channel, second=channel)

    with pytest.raises(SequenceNotApplicableError, match="different channels"):
        asyncio.run(executor._execute_step(ctx, asyncio.Event(), _noop_probe))


def test_do_pair_rejects_non_integer_state() -> None:
    executor = SequenceExecutor()
    ctx = _pair_context(state2b="bad-value")

    with pytest.raises(SequenceNotApplicableError, match="integer in range 0..3"):
        asyncio.run(executor._execute_step(ctx, asyncio.Event(), _noop_probe))


def test_do_pair_rejects_out_of_range_state() -> None:
    executor = SequenceExecutor()
    ctx = _pair_context(state2b=4)

    with pytest.raises(SequenceNotApplicableError, match="range 0..3"):
        asyncio.run(executor._execute_step(ctx, asyncio.Event(), _noop_probe))


def test_do_pair_enqueues_pair_command(monkeypatch: pytest.MonkeyPatch) -> None:
    executor = SequenceExecutor()
    ctx = _pair_context(state2b=2)
    captured: dict[str, object] = {}

    async def _fake_enqueue_do_command(**kwargs):
        captured.update(kwargs)

    monkeypatch.setattr("app.services.sequence_executor.enqueue_do_command", _fake_enqueue_do_command)

    asyncio.run(executor._execute_step(ctx, asyncio.Event(), _noop_probe))

    assert captured == {
        "unit_id": "DO-001",
        "mode": Cmd.SET_PAIR_BIT,
        "chA": 2,
        "chB": 3,
        "state2b": 2,
    }
