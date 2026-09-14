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

    async def get(self, key: str):
        return self.values.get(key)

    async def eval(self, _script: str, _count: int, *args):
        if _count > 1:
            lease_keys = list(args[: _count // 2])
            epoch_keys = list(args[_count // 2 : _count])
            owner_kind, owner_id, lease_id, _ttl, _size = args[_count:]
            if any(key in self.values for key in lease_keys):
                return []
            epochs = []
            for lease_key, epoch_key in zip(lease_keys, epoch_keys):
                self.epochs[epoch_key] = self.epochs.get(epoch_key, 0) + 1
                epochs.append(self.epochs[epoch_key])
                self.values[lease_key] = json.dumps({"lease_id": lease_id, "owner_kind": owner_kind, "owner_id": owner_id})
            return epochs
        key, lease_id = args
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


@pytest.mark.anyio
async def test_lease_fence_rejects_stale_owner() -> None:
    redis = _FakeRedis()
    admission = HardwareCommandAdmission(redis)
    first = await admission.acquire(channel_id=12, owner_kind="fat", owner_id="run-1")
    assert first is not None
    assert await admission.is_current(first) is True
    redis.values[admission._key(12)] = json.dumps(
        {"lease_id": "other", "owner_kind": "manual", "owner_id": "session-2", "fencing_epoch": 2}
    )
    assert await admission.is_current(first) is False


@pytest.mark.anyio
async def test_multi_channel_lease_is_atomic() -> None:
    redis = _FakeRedis()
    admission = HardwareCommandAdmission(redis)

    first = await admission.acquire_many(channel_ids=[12, 13], owner_kind="sequence", owner_id="run-1")
    second = await admission.acquire_many(channel_ids=[13, 14], owner_kind="manual", owner_id="session-2")

    assert first is not None
    assert [lease.channel_id for lease in first] == [12, 13]
    assert second is None
    assert await admission.release(first[0]) is True
    assert await admission.release(first[1]) is True
