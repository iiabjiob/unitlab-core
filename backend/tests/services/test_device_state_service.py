from __future__ import annotations

import asyncio
from collections.abc import Awaitable
from typing import TypeVar, cast

import pytest

from app.infrastructure.protocol.modes import State
from app.infrastructure.protocol.header import PacketHeader
from app.services.device_state_service import DeviceStateService


class Redis:
    def __init__(self, bitmask: str | None) -> None:
        self.bitmask: str | None = bitmask
        self.writes: list[tuple[str, str]] = []

    async def get(self, key: str) -> str | None:
        if key.endswith(":bitmask"):
            return self.bitmask
        return "do"

    async def set(self, key: str, value: object) -> None:
        self.writes.append((key, str(value)))


class Decoded:
    def __init__(self, **values: object) -> None:
        self.__dict__.update(values)

    def model_dump(self) -> dict[str, object]:
        return dict(self.__dict__)


_T = TypeVar("_T")


def run_async(awaitable: Awaitable[_T]) -> _T:
    return asyncio.run(awaitable)


def _header(mode: State, packet_id: int | None = None) -> PacketHeader:
    return PacketHeader(int(mode), 1, cast(int, packet_id), 1, 0, 0)


def test_single_bit_response_does_not_create_full_mask_from_zero(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = Redis(None)
    monkeypatch.setattr(
        "app.services.device_state_service.RedisManager.get_instance",
        lambda: redis,
    )

    changed, event = run_async(
        DeviceStateService.update_state(
            "unit-1",
            _header(State.STATE_SINGLE_BIT),
            Decoded(ch=2, value=1),
        )
    )

    assert changed is False
    assert event is not None
    assert redis.writes == []


def test_changed_bit_response_does_not_create_full_mask_from_zero(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = Redis("invalid")
    monkeypatch.setattr(
        "app.services.device_state_service.RedisManager.get_instance",
        lambda: redis,
    )

    changed, event = run_async(
        DeviceStateService.update_state(
            "unit-1",
            _header(State.STATE_CHANGED_BIT),
            Decoded(changed=1, state=1),
        )
    )

    assert changed is False
    assert event is None
    assert redis.writes == []


def test_single_bit_response_updates_existing_full_mask(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = Redis("2")
    monkeypatch.setattr(
        "app.services.device_state_service.RedisManager.get_instance",
        lambda: redis,
    )

    changed, event = run_async(
        DeviceStateService.update_state(
            "unit-1",
            _header(State.STATE_SINGLE_BIT),
            Decoded(ch=0, value=1),
        )
    )

    assert changed is True
    assert event is not None
    assert redis.writes == [("device:unit-1:bitmask", "3")]


def test_state_response_records_request_packet_id_without_changing_legacy_fixture(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = Redis("2")
    monkeypatch.setattr(
        "app.services.device_state_service.RedisManager.get_instance",
        lambda: redis,
    )

    _ = run_async(
        DeviceStateService.update_state(
            "unit-1",
            _header(State.STATE_SINGLE_BIT, packet_id=44),
            Decoded(ch=0, value=1),
        )
    )

    assert redis.writes[-1] == ("device:unit-1:last_state_packet_id", "44")
