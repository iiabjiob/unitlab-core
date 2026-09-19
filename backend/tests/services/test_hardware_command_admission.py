from __future__ import annotations

import json
from typing import cast

import pytest
import redis.asyncio as redis_asyncio

from app.services.hardware_command_admission import HardwareCommandAdmission


class _FakeRedis:
    def __init__(self) -> None:
        self.values: dict[str, str] = {}
        self.epochs: dict[str, int] = {}

    async def incr(self, key: str) -> int:
        self.epochs[key] = self.epochs.get(key, 0) + 1
        return self.epochs[key]

    async def set(self, key: str, value: str, **kwargs: object) -> bool:
        if kwargs.get("nx") and key in self.values:
            return False
        self.values[key] = value
        return True

    async def get(self, key: str) -> str | None:
        return self.values.get(key)

    async def eval(self, _script: str, _count: int, *args: object) -> list[int] | int:
        if _count > 1:
            lease_keys = [str(key) for key in args[: _count // 2]]
            epoch_keys = [str(key) for key in args[_count // 2 : _count]]
            owner_kind, owner_id, lease_id, _ttl, _size = (str(value) for value in args[_count:])
            if any(key in self.values for key in lease_keys):
                return []
            epochs: list[int] = []
            for lease_key, epoch_key in zip(lease_keys, epoch_keys):
                self.epochs[epoch_key] = self.epochs.get(epoch_key, 0) + 1
                epoch = self.epochs[epoch_key]
                epochs.append(epoch)
                self.values[lease_key] = json.dumps(
                    {
                        "lease_id": lease_id,
                        "owner_kind": owner_kind,
                        "owner_id": owner_id,
                        "fencing_epoch": epoch,
                    }
                )
            return epochs
        key, lease_id = (str(value) for value in args)
        value = self.values.get(key)
        if value and lease_id in value:
            del self.values[key]
            return 1
        return 0


@pytest.mark.anyio
async def test_channel_lease_is_exclusive_and_fenced() -> None:
    redis = _FakeRedis()
    admission = HardwareCommandAdmission(cast(redis_asyncio.Redis, cast(object, redis)))

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
    admission = HardwareCommandAdmission(cast(redis_asyncio.Redis, cast(object, redis)))
    first = await admission.acquire(channel_id=12, owner_kind="fat", owner_id="run-1")
    assert first is not None
    redis.values[admission._key(12)] = json.dumps({"lease_id": "other"})  # pyright: ignore[reportPrivateUsage]

    assert await admission.release(first) is False
    assert admission._key(12) in redis.values  # pyright: ignore[reportPrivateUsage]


@pytest.mark.anyio
async def test_lease_fence_rejects_stale_owner() -> None:
    redis = _FakeRedis()
    admission = HardwareCommandAdmission(cast(redis_asyncio.Redis, cast(object, redis)))
    first = await admission.acquire(channel_id=12, owner_kind="fat", owner_id="run-1")
    assert first is not None
    assert await admission.is_current(first) is True
    redis.values[admission._key(12)] = json.dumps(  # pyright: ignore[reportPrivateUsage]
        {"lease_id": "other", "owner_kind": "manual", "owner_id": "session-2", "fencing_epoch": 2}
    )
    assert await admission.is_current(first) is False


@pytest.mark.anyio
async def test_multi_channel_lease_is_atomic() -> None:
    redis = _FakeRedis()
    admission = HardwareCommandAdmission(cast(redis_asyncio.Redis, cast(object, redis)))

    first = await admission.acquire_many(channel_ids=[12, 13], owner_kind="sequence", owner_id="run-1")
    second = await admission.acquire_many(channel_ids=[13, 14], owner_kind="manual", owner_id="session-2")

    assert first is not None
    assert [lease.channel_id for lease in first] == [12, 13]
    assert second is None
    assert await admission.release(first[0]) is True
    assert await admission.release(first[1]) is True


@pytest.mark.anyio
async def test_multi_channel_lease_fence_is_current() -> None:
    redis = _FakeRedis()
    admission = HardwareCommandAdmission(cast(redis_asyncio.Redis, cast(object, redis)))

    leases = await admission.acquire_many(channel_ids=[12, 13], owner_kind="manual", owner_id="session-1")

    assert leases is not None
    current = [await admission.is_current(lease) for lease in leases]
    assert current == [True, True]
