from __future__ import annotations

from datetime import datetime, timezone

from app.api.v1.signal_sheet.repository import (
    _is_channel_compatible,
    _parse_tested_at,
    _pick_candidate_channel,
    _required_channel_type,
)
from app.models.channel import Channel
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
