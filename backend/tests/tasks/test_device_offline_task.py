from __future__ import annotations

import asyncio

import pytest

from app.tasks import device_offline_task


class FakeRedis:
    def __init__(self) -> None:
        self.writes: list[tuple[str, str]] = []

    async def smembers(self, _key: str) -> set[bytes]:
        return {b"unit-1"}

    async def get(self, key: str) -> bytes | None:
        if key.endswith(":status"):
            return b"online"
        return None

    async def set(self, key: str, value: str, **_kwargs: object) -> None:
        self.writes.append((key, value))


@pytest.mark.anyio
async def test_checker_does_not_mark_device_offline_during_clock_adjustment(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = FakeRedis()

    async def _active() -> bool:
        return True

    async def _grace_active(_redis: object) -> bool:
        return await _active()

    monkeypatch.setattr(device_offline_task.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(device_offline_task, "is_clock_adjustment_grace_active", _grace_active)

    async def stop(_seconds: int) -> None:
        raise asyncio.CancelledError

    monkeypatch.setattr(device_offline_task.asyncio, "sleep", stop)  # pyright: ignore[reportPrivateLocalImportUsage]

    with pytest.raises(asyncio.CancelledError):
        await device_offline_task.device_offline_checker()

    assert redis.writes == []


@pytest.mark.anyio
async def test_checker_requires_two_consecutive_missing_heartbeats(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = FakeRedis()
    monkeypatch.setattr(device_offline_task.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]

    checks = 0

    async def inactive(_redis: object) -> bool:
        return False

    async def sleep(_seconds: int) -> None:
        nonlocal checks
        checks += 1
        if checks >= 2:
            raise asyncio.CancelledError

    monkeypatch.setattr(device_offline_task, "is_clock_adjustment_grace_active", inactive)
    monkeypatch.setattr(device_offline_task.asyncio, "sleep", sleep)  # pyright: ignore[reportPrivateLocalImportUsage]

    with pytest.raises(asyncio.CancelledError):
        await device_offline_task.device_offline_checker()

    assert redis.writes == [("device:unit-1:status", "offline")]
