from __future__ import annotations

import json
from unittest.mock import AsyncMock

import pytest

from app.services import core_ntp_event_forwarder as forwarder


class FakeRedis:
    def __init__(self) -> None:
        self.acked: list[str] = []

    async def xack(self, _stream: str, _group: str, entry_id: str):
        self.acked.append(entry_id)


@pytest.mark.anyio
async def test_state_clock_step_marks_presence_grace(monkeypatch):
    redis = FakeRedis()
    mark_grace = AsyncMock()
    publish = AsyncMock()
    monkeypatch.setattr(forwarder, "mark_clock_adjustment_grace", mark_grace)
    monkeypatch.setattr(forwarder.WsEventPublisher, "publish", publish)
    monotonic_calls = 0

    def monotonic() -> float:
        nonlocal monotonic_calls
        monotonic_calls += 1
        return 100.0 if monotonic_calls == 1 else 110.0

    monkeypatch.setattr(forwarder.time, "monotonic", monotonic)
    monkeypatch.setattr(forwarder, "_last_state_system_time", None)
    monkeypatch.setattr(forwarder, "_last_state_monotonic", None)

    entries = [
        ("1-0", {"json": json.dumps({"event": "state", "updated_at": "2026-01-01T00:00:00+00:00"})}),
        ("2-0", {"json": json.dumps({"event": "state", "updated_at": "2026-01-01T00:00:20+00:00"})}),
    ]
    await forwarder._process_entries(redis, entries)

    mark_grace.assert_awaited_once_with()
    assert redis.acked == ["1-0", "2-0"]
    assert publish.await_count == 2
