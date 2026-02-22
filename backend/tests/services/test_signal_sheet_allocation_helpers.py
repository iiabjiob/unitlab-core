from __future__ import annotations

from datetime import datetime, timedelta, timezone

from app.api.v1.signal_sheet.repository import (
    _channel_auto_allocate_sort_key,
    _is_channel_compatible,
    _parse_tested_at,
    _pick_candidate_channel,
    _required_channel_type,
)
from app.models.channel import Channel
from app.models.device import Device
from app.models.signal import SignalIODirection


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
