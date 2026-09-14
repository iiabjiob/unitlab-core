from __future__ import annotations

import asyncio
from types import SimpleNamespace

from app.infrastructure.protocol.modes import State
from app.services.device_state_service import DeviceStateService


class Redis:
    def __init__(self, bitmask=None) -> None:
        self.bitmask = bitmask
        self.writes: list[tuple[str, str]] = []

    async def get(self, key: str):
        if key.endswith(":bitmask"):
            return self.bitmask
        return "do"

    async def set(self, key: str, value) -> None:
        self.writes.append((key, str(value)))


class Decoded:
    def __init__(self, **values) -> None:
        self.__dict__.update(values)

    def model_dump(self):
        return dict(self.__dict__)


def run_async(awaitable):
    return asyncio.run(awaitable)


def test_single_bit_response_does_not_create_full_mask_from_zero(monkeypatch) -> None:
    redis = Redis(None)
    monkeypatch.setattr(
        "app.services.device_state_service.RedisManager.get_instance",
        lambda: redis,
    )

    changed, event = run_async(
        DeviceStateService.update_state(
            "unit-1",
            SimpleNamespace(mode=State.STATE_SINGLE_BIT, timestamp_ms=1),
            Decoded(ch=2, value=1),
        )
    )

    assert changed is False
    assert event is not None
    assert redis.writes == []


def test_changed_bit_response_does_not_create_full_mask_from_zero(monkeypatch) -> None:
    redis = Redis("invalid")
    monkeypatch.setattr(
        "app.services.device_state_service.RedisManager.get_instance",
        lambda: redis,
    )

    changed, event = run_async(
        DeviceStateService.update_state(
            "unit-1",
            SimpleNamespace(mode=State.STATE_CHANGED_BIT, timestamp_ms=1),
            Decoded(changed=1, state=1),
        )
    )

    assert changed is False
    assert event is None
    assert redis.writes == []


def test_single_bit_response_updates_existing_full_mask(monkeypatch) -> None:
    redis = Redis("2")
    monkeypatch.setattr(
        "app.services.device_state_service.RedisManager.get_instance",
        lambda: redis,
    )

    changed, event = run_async(
        DeviceStateService.update_state(
            "unit-1",
            SimpleNamespace(mode=State.STATE_SINGLE_BIT, timestamp_ms=1),
            Decoded(ch=0, value=1),
        )
    )

    assert changed is True
    assert event is not None
    assert redis.writes == [("device:unit-1:bitmask", "3")]


def test_state_response_records_request_packet_id_without_changing_legacy_fixture(monkeypatch) -> None:
    redis = Redis("2")
    monkeypatch.setattr(
        "app.services.device_state_service.RedisManager.get_instance",
        lambda: redis,
    )

    run_async(
        DeviceStateService.update_state(
            "unit-1",
            SimpleNamespace(mode=State.STATE_SINGLE_BIT, timestamp_ms=1, packet_id=44),
            Decoded(ch=0, value=1),
        )
    )

    assert ("device:unit-1:last_state_packet_id", "44") in redis.writes
