from __future__ import annotations

import asyncio
from datetime import datetime, timedelta, timezone
from types import SimpleNamespace

from app.api.v1.signal_sheet import repository as signal_sheet_repository
from app.api.v1.signal_sheet.repository import (
    _build_allocation_health,
    _channel_auto_allocate_sort_key,
    _is_channel_compatible,
    _parse_tested_at,
    _pick_candidate_channel,
    _resolve_allocation_status,
    _required_channel_type,
    SignalSheetRepository,
)
from app.models.channel import Channel
from app.models.device import Device
from app.models.signal import Signal, SignalIODirection
from app.models.signal_sheet import SignalAllocation


def run_async(awaitable):
    return asyncio.run(awaitable)


def test_required_channel_type_maps_inverse_direction() -> None:
    assert _required_channel_type("DI") == "do"
    assert _required_channel_type("DO") == "di"
    assert _required_channel_type("AI") == "ao"
    assert _required_channel_type("AO") == "ai"
    assert _required_channel_type("unknown") is None


def test_channel_compatibility_uses_inverse_mapping() -> None:
    assert _is_channel_compatible(SignalIODirection.DI, "do") is True
    assert _is_channel_compatible(SignalIODirection.DO, "di") is True
    assert _is_channel_compatible(SignalIODirection.AI, "ao") is True
    assert _is_channel_compatible(SignalIODirection.AO, "ai") is True

    assert _is_channel_compatible(SignalIODirection.DI, "di") is False
    assert _is_channel_compatible(SignalIODirection.DO, "do") is False


def test_allocation_health_marks_unassigned_as_clean() -> None:
    signal = Signal(id=1, workspace_id=1, key="S1", name="Signal 1", io_direction=SignalIODirection.DI)

    health = _build_allocation_health(
        signal=signal,
        allocation=None,
        channel=None,
        unit_online=None,
    )

    assert health == {
        "conflict": False,
        "invalid_type": False,
        "missing_device": False,
        "missing_channel": False,
        "offline_device": False,
        "stale_device": False,
    }
    assert _resolve_allocation_status(None, health) == "unassigned"


def test_allocation_health_marks_invalid_type() -> None:
    signal = Signal(id=1, workspace_id=1, key="S1", name="Signal 1", io_direction=SignalIODirection.DI)
    allocation = SignalAllocation(id=10, workspace_id=1, signal_id=1, channel_id=20)
    channel = Channel(
        id=20,
        device_id=2,
        channel_index=0,
        channel_type="di",
        device=Device(id=2, unit_id="unit-a"),
    )

    health = _build_allocation_health(
        signal=signal,
        allocation=allocation,
        channel=channel,
        unit_online=True,
    )

    assert health["invalid_type"] is True
    assert _resolve_allocation_status(allocation, health) == "invalid"


def test_allocation_health_marks_missing_channel_before_assigned() -> None:
    signal = Signal(id=1, workspace_id=1, key="S1", name="Signal 1", io_direction=SignalIODirection.DI)
    allocation = SignalAllocation(id=10, workspace_id=1, signal_id=1, channel_id=20)

    health = _build_allocation_health(
        signal=signal,
        allocation=allocation,
        channel=None,
        unit_online=None,
    )

    assert health["missing_channel"] is True
    assert _resolve_allocation_status(allocation, health) == "missing"


def test_allocation_health_tracks_offline_assigned_channel() -> None:
    signal = Signal(id=1, workspace_id=1, key="S1", name="Signal 1", io_direction=SignalIODirection.DI)
    allocation = SignalAllocation(id=10, workspace_id=1, signal_id=1, channel_id=20)
    channel = Channel(
        id=20,
        device_id=2,
        channel_index=0,
        channel_type="do",
        device=Device(id=2, unit_id="unit-a"),
    )

    health = _build_allocation_health(
        signal=signal,
        allocation=allocation,
        channel=channel,
        unit_online=False,
    )

    assert health["offline_device"] is True
    assert _resolve_allocation_status(allocation, health) == "assigned"


def test_parse_tested_at_parses_iso_utc_suffix() -> None:
    payload = {"tested_at": "2026-02-13T12:34:56Z"}
    parsed = _parse_tested_at(payload)

    assert parsed is not None
    assert parsed.tzinfo is not None
    assert parsed == datetime(2026, 2, 13, 12, 34, 56, tzinfo=timezone.utc)


def test_pick_candidate_channel_skips_used_ids() -> None:
    first = Channel(id=1, device_id=1, channel_index=0, channel_type="do")
    second = Channel(id=2, device_id=1, channel_index=1, channel_type="do")

    candidate = _pick_candidate_channel(candidates=[first, second], used_channel_ids={1})

    assert candidate is not None
    assert candidate.id == 2


def test_pick_candidate_channel_prefers_online_unit_when_enabled() -> None:
    offline_device = Device(id=1, unit_id="unit-offline", last_seen_at=None)
    online_device = Device(
        id=2,
        unit_id="unit-online",
        last_seen_at=datetime.now(timezone.utc) - timedelta(seconds=1),
    )
    offline = Channel(
        id=10,
        device_id=1,
        channel_index=0,
        channel_type="do",
        device=offline_device,
    )
    online = Channel(
        id=20,
        device_id=2,
        channel_index=1,
        channel_type="do",
        device=online_device,
    )

    candidate = _pick_candidate_channel(
        candidates=[offline, online],
        used_channel_ids=set(),
        prefer_online=True,
    )

    assert candidate is not None
    assert candidate.id == 20


def test_pick_candidate_channel_falls_back_when_no_online_available() -> None:
    stale_seen = datetime.now(timezone.utc) - timedelta(hours=2)
    first = Channel(
        id=30,
        device_id=1,
        channel_index=0,
        channel_type="do",
        device=Device(id=1, unit_id="unit-a", last_seen_at=stale_seen),
    )
    second = Channel(
        id=40,
        device_id=2,
        channel_index=1,
        channel_type="do",
        device=Device(id=2, unit_id="unit-b", last_seen_at=None),
    )

    candidate = _pick_candidate_channel(
        candidates=[first, second],
        used_channel_ids=set(),
        prefer_online=True,
    )

    assert candidate is not None
    assert candidate.id == 30


def test_channel_auto_allocate_sort_key_prefers_online_then_channel_index() -> None:
    offline_device = Device(id=1, unit_id="unit-offline", last_seen_at=None)
    online_device = Device(
        id=2,
        unit_id="unit-online",
        last_seen_at=datetime.now(timezone.utc) - timedelta(seconds=1),
    )
    channels = [
        Channel(id=101, device_id=1, channel_index=0, channel_type="do", device=offline_device),
        Channel(id=202, device_id=2, channel_index=2, channel_type="do", device=online_device),
        Channel(id=201, device_id=2, channel_index=1, channel_type="do", device=online_device),
    ]

    ordered = sorted(
        channels,
        key=lambda channel: _channel_auto_allocate_sort_key(channel, prefer_online=True),
    )

    assert [channel.id for channel in ordered] == [201, 202, 101]


def test_channel_auto_allocate_sort_key_without_online_priority() -> None:
    offline_device = Device(id=1, unit_id="unit-offline", last_seen_at=None)
    online_device = Device(
        id=2,
        unit_id="unit-online",
        last_seen_at=datetime.now(timezone.utc) - timedelta(seconds=1),
    )
    channels = [
        Channel(id=202, device_id=2, channel_index=2, channel_type="do", device=online_device),
        Channel(id=101, device_id=1, channel_index=0, channel_type="do", device=offline_device),
        Channel(id=201, device_id=2, channel_index=1, channel_type="do", device=online_device),
    ]

    ordered = sorted(
        channels,
        key=lambda channel: _channel_auto_allocate_sort_key(channel, prefer_online=False),
    )

    assert [channel.id for channel in ordered] == [101, 201, 202]


def test_auto_allocate_applies_free_compatible_channel(monkeypatch) -> None:
    class FakeDb:
        def __init__(self) -> None:
            self.added: list[SignalAllocation] = []
            self.flushed = 0

        def add(self, item) -> None:
            self.added.append(item)

        async def flush(self) -> None:
            self.flushed += 1

    db = FakeDb()
    repo = SignalSheetRepository(db=db)  # type: ignore[arg-type]
    signal = Signal(id=1, workspace_id=1, key="S1", name="Signal 1", io_direction=SignalIODirection.DI)
    channel = Channel(
        id=20,
        device_id=2,
        channel_index=0,
        channel_type="do",
        device=Device(id=2, unit_id="unit-a"),
    )

    async def active_signals_by_ids(workspace_id: int, signal_ids: set[int]):
        return {1: signal}

    async def allocations_by_signal_id(workspace_id: int):
        return {}

    async def list_channels():
        return [channel]

    class FakePresenceService:
        async def get_presence_map(self, unit_ids):
            return {"unit-a": SimpleNamespace(online=True)}

    monkeypatch.setattr(repo, "_active_signals_by_ids", active_signals_by_ids)
    monkeypatch.setattr(repo, "_allocations_by_signal_id", allocations_by_signal_id)
    monkeypatch.setattr(repo, "_list_channels", list_channels)
    async def cleanup_orphan_allocations(workspace_id: int):
        return None

    monkeypatch.setattr(repo, "cleanup_orphan_allocations", cleanup_orphan_allocations)
    monkeypatch.setattr(signal_sheet_repository, "DevicePresenceService", lambda: FakePresenceService())

    result = run_async(
        repo.auto_allocate(
            workspace_id=1,
            signal_ids=[1],
            prefer_online=True,
            prefer_single_unit=False,
            overwrite_existing=False,
            commit=False,
        )
    )

    assert result.assigned == 1
    assert result.skipped == 0
    assert result.changed_signal_ids == [1]
    assert result.skipped_items == []
    assert result.rejected == []
    assert db.added[0].signal_id == 1
    assert db.added[0].channel_id == 20
    assert db.flushed == 1


def test_auto_allocate_returns_skipped_reason_when_no_channel_available(monkeypatch) -> None:
    class FakeDb:
        async def flush(self) -> None:
            return None

    repo = SignalSheetRepository(db=FakeDb())  # type: ignore[arg-type]
    signal = Signal(id=1, workspace_id=1, key="S1", name="Signal 1", io_direction=SignalIODirection.DI)

    async def active_signals_by_ids(workspace_id: int, signal_ids: set[int]):
        return {1: signal}

    async def allocations_by_signal_id(workspace_id: int):
        return {}

    async def list_channels():
        return []

    monkeypatch.setattr(repo, "_active_signals_by_ids", active_signals_by_ids)
    monkeypatch.setattr(repo, "_allocations_by_signal_id", allocations_by_signal_id)
    monkeypatch.setattr(repo, "_list_channels", list_channels)
    async def cleanup_orphan_allocations(workspace_id: int):
        return None

    monkeypatch.setattr(repo, "cleanup_orphan_allocations", cleanup_orphan_allocations)

    result = run_async(
        repo.auto_allocate(
            workspace_id=1,
            signal_ids=[1],
            prefer_online=True,
            prefer_single_unit=False,
            overwrite_existing=False,
            commit=False,
        )
    )

    assert result.assigned == 0
    assert result.skipped == 0
    assert result.unassigned_signal_ids == [1]
    assert result.changed_signal_ids == []
    assert result.skipped_items[0].code == "no_channel_available"
