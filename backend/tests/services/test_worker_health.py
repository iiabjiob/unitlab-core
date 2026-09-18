import json
from unittest.mock import AsyncMock

import pytest

from app.services import worker_health as health


@pytest.fixture
def anyio_backend():
    return "asyncio"


@pytest.mark.anyio
@pytest.mark.parametrize("wall_time", ["2000-01-01T00:00:00+00:00", "2099-01-01T00:00:00+00:00"])
@pytest.mark.parametrize("age,boot,status", [(2, "boot", "online"), (100, "boot", "offline"), (2, "old-boot", "offline"), (-2, "boot", "offline")])
async def test_liveness_uses_monotonic_age_and_boot_identity(monkeypatch, wall_time, age, boot, status):
    redis = AsyncMock()
    redis.exists.return_value = False
    redis.ttl.return_value = -1
    redis.get.return_value = json.dumps({"status": "online", "updated_at": wall_time, "boot_id": boot, "monotonic_at": 100 - age})
    monkeypatch.setattr(health.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(health, "_host_boot_id", lambda: "boot")
    monkeypatch.setattr(health.time, "monotonic", lambda: 100)
    workers = await health.collect_worker_health()
    assert all(worker.status == status for worker in workers)


@pytest.mark.anyio
async def test_heartbeat_does_not_expire_on_wall_clock_step(monkeypatch):
    redis = AsyncMock()
    monkeypatch.setattr(health.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(health, "_host_boot_id", lambda: "boot")
    monkeypatch.setattr(health.time, "monotonic", lambda: 100)
    await health.write_worker_status("sequence_runner", status="degraded", detail="actual failure")
    args, kwargs = redis.set.call_args
    assert kwargs == {}
    payload = json.loads(args[1])
    assert payload["monotonic_at"] == 100
    assert payload["boot_id"] == "boot"
    redis.get.return_value = args[1]
    redis.ttl.return_value = -1
    redis.exists.return_value = False
    assert all(worker.status == "degraded" for worker in await health.collect_worker_health())


@pytest.mark.anyio
async def test_stale_worker_is_offline_even_during_clock_grace(monkeypatch):
    redis = AsyncMock()
    redis.exists.return_value = True
    redis.ttl.return_value = -1
    redis.get.return_value = json.dumps({"status": "online", "boot_id": "boot", "monotonic_at": 1})
    monkeypatch.setattr(health.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(health, "_host_boot_id", lambda: "boot")
    monkeypatch.setattr(health.time, "monotonic", lambda: 100)
    assert all(worker.status == "offline" for worker in await health.collect_worker_health())


@pytest.mark.anyio
async def test_clock_adjustment_grace_uses_monotonic_deadline(monkeypatch):
    redis = AsyncMock()
    monkeypatch.setattr(health.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(health, "_host_boot_id", lambda: "boot")
    monkeypatch.setattr(health.time, "monotonic", lambda: 100)

    await health.mark_clock_adjustment_grace(seconds=90)
    payload = json.loads(redis.set.call_args.args[1])
    assert payload == {"boot_id": "boot", "expires_monotonic": 190}

    redis.get.return_value = redis.set.call_args.args[1]
    assert await health.is_clock_adjustment_grace_active(redis) is True
    monkeypatch.setattr(health.time, "monotonic", lambda: 190)
    assert await health.is_clock_adjustment_grace_active(redis) is False
