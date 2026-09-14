from __future__ import annotations

import json

import pytest

from app.services.hardware_command_admission import HardwareCommandAdmission


class _FakeRedis:
    def __init__(self) -> None:
        self.values: dict[str, str] = {}
        self.epochs: dict[str, int] = {}

    async def incr(self, key: str) -> int:
        self.epochs[key] = self.epochs.get(key, 0) + 1
        return self.epochs[key]

    async def set(self, key: str, value: str, **kwargs):
        if kwargs.get("nx") and key in self.values:
            return False
        self.values[key] = value
        return True

    async def eval(self, _script: str, _count: int, key: str, lease_id: str):
        value = self.values.get(key)
        if value and lease_id in value:
            del self.values[key]
            return 1
        return 0


@pytest.mark.anyio
async def test_channel_lease_is_exclusive_and_fenced() -> None:
    redis = _FakeRedis()
    admission = HardwareCommandAdmission(redis)

    first = await admission.acquire(channel_id=12, owner_kind="fat", owner_id="run-1")
    second = await admission.acquire(channel_id=12, owner_kind="manual", owner_id="session-2")

    assert first is not None
    assert second is None
    assert first.fencing_epoch == 1
    assert await admission.release(first) is True
    assert await admission.acquire(channel_id=12, owner_kind="manual", owner_id="session-2") is not None


@pytest.mark.anyio
async def test_release_cannot_remove_another_owner() -> None:
    redis = _FakeRedis()
    admission = HardwareCommandAdmission(redis)
    first = await admission.acquire(channel_id=12, owner_kind="fat", owner_id="run-1")
    assert first is not None
    redis.values[admission._key(12)] = json.dumps({"lease_id": "other"})

    assert await admission.release(first) is False
    assert admission._key(12) in redis.values
