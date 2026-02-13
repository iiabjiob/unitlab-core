from __future__ import annotations

import asyncio
from datetime import datetime, timedelta, timezone

from app.models.channel import Channel
from app.models.device import Device
from app.models.signal import Signal, SignalIODirection
from app.models.test_run import TestRun, TestRunAllocation, TestRunAllocationEntry
from app.schemas.test_run_schema import TestRunPreflightSchema
from app.services.test_run_preflight_service import TestRunPreflightService


class _RepoStub:
    def __init__(self, channels: list[Channel]):
        self._channels = channels

    async def get_channels_by_ids(self, channel_ids: set[int]) -> dict[int, Channel]:
        return {channel.id: channel for channel in self._channels if channel.id in channel_ids}

    async def list_channels(self) -> list[Channel]:
        return list(self._channels)


def test_preflight_reports_reallocation_candidates_for_offline_unit() -> None:
    now = datetime.now(timezone.utc)

    offline_device = Device(id=1, unit_id="DO-UNIT-1", last_seen_at=now - timedelta(hours=2))
    online_device = Device(id=2, unit_id="DO-UNIT-2", last_seen_at=now)

    offline_channel = Channel(id=10, device_id=1, channel_index=0, channel_type="DO")
    offline_channel.device = offline_device
    spare_channel = Channel(id=20, device_id=2, channel_index=1, channel_type="DO")
    spare_channel.device = online_device

    signal = Signal(
        id=100,
        workspace_id=1,
        key="breaker_close",
        name="Breaker Close",
        io_direction=SignalIODirection.DO,
        signal_metadata={},
    )

    entry = TestRunAllocationEntry(id=500, channel_id=10, signal_id=100)
    entry.channel = offline_channel
    entry.signal = signal

    allocation = TestRunAllocation(id=300, test_run_id=200, notes=None)
    allocation.entries = [entry]

    run = TestRun(id=200, workspace_id=1)
    run.allocation = allocation

    repo = _RepoStub([offline_channel, spare_channel])
    result: TestRunPreflightSchema = asyncio.run(TestRunPreflightService.evaluate(repo, run))

    assert result.ready is False
    assert result.reallocation_required is True
    assert result.entries[0].available is False
    assert result.entries[0].recommended_channel_ids == [20]


def test_preflight_ready_when_all_units_alive() -> None:
    now = datetime.now(timezone.utc)

    device = Device(id=3, unit_id="DI-UNIT", last_seen_at=now)
    channel = Channel(id=30, device_id=3, channel_index=0, channel_type="DI")
    channel.device = device

    signal = Signal(
        id=101,
        workspace_id=1,
        key="breaker_feedback",
        name="Breaker Feedback",
        io_direction=SignalIODirection.DI,
        signal_metadata={},
    )

    entry = TestRunAllocationEntry(id=501, channel_id=30, signal_id=101)
    entry.channel = channel
    entry.signal = signal

    allocation = TestRunAllocation(id=301, test_run_id=201, notes=None)
    allocation.entries = [entry]

    run = TestRun(id=201, workspace_id=1)
    run.allocation = allocation

    repo = _RepoStub([channel])
    result: TestRunPreflightSchema = asyncio.run(TestRunPreflightService.evaluate(repo, run))

    assert result.ready is True
    assert result.reallocation_required is False
    assert result.entries[0].available is True
