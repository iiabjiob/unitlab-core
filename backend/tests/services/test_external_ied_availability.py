from __future__ import annotations

import asyncio
from dataclasses import dataclass

import pytest

from app.schemas.verification_schema import VerificationMmsReachabilityResultSchema
from app.services import external_ied_availability as availability
from app.services.external_ied_availability import configure_external_ied_targets, run_external_ied_availability_checker


class _FakePipeline:
    def __init__(self, redis: "_FakeRedis") -> None:
        self.redis = redis
        self.ops: list[tuple[str, tuple]] = []

    def delete(self, *keys: str) -> None:
        self.ops.append(("delete", keys))

    def hset(self, key: str, field: str, value: str) -> None:
        self.ops.append(("hset", (key, field, value)))

    async def execute(self) -> None:
        for op, args in self.ops:
            if op == "delete":
                await self.redis.delete(*args)
            elif op == "hset":
                await self.redis.hset(*args)


class _FakeRedis:
    def __init__(self) -> None:
        self.hashes: dict[str, dict[str, str]] = {}
        self.sets: dict[str, set[str]] = {}
        self.values: dict[str, str] = {}

    async def hgetall(self, key: str) -> dict[str, str]:
        return dict(self.hashes.get(key, {}))

    async def hset(self, key: str, field: str, value: str) -> None:
        self.hashes.setdefault(key, {})[field] = value

    async def sadd(self, key: str, value: str) -> None:
        self.sets.setdefault(key, set()).add(value)

    async def srem(self, key: str, value: str) -> None:
        self.sets.setdefault(key, set()).discard(value)

    async def smembers(self, key: str) -> set[str]:
        return set(self.sets.get(key, set()))

    async def delete(self, *keys: str) -> None:
        for key in keys:
            self.hashes.pop(key, None)
            self.sets.pop(key, None)
            self.values.pop(key, None)

    async def set(self, key: str, value: str, **_kwargs) -> bool:
        if _kwargs.get("nx") and key in self.values:
            return False
        self.values[key] = value
        return True

    def pipeline(self) -> _FakePipeline:
        return _FakePipeline(self)


@dataclass
class _Published:
    events: list[object]

    async def publish(self, event: object) -> None:
        self.events.append(event)


@pytest.mark.anyio
async def test_configure_external_ied_targets_writes_expected_snapshot(monkeypatch) -> None:
    redis = _FakeRedis()
    published = _Published([])
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(availability.WsEventPublisher, "publish", published.publish)

    event = await configure_external_ied_targets(7, [
        {"ip": "10.10.10.20", "signal_ids": [2, 1, 1]},
    ])

    assert event.workspace_id == 7
    assert event.devices[0].ip == "10.10.10.20"
    assert event.devices[0].port == 102
    assert event.devices[0].status == "expected"
    assert event.devices[0].signal_ids == [1, 2]
    assert published.events[-1] == event


@pytest.mark.anyio
async def test_configure_external_ied_targets_clears_backend_context(monkeypatch) -> None:
    redis = _FakeRedis()
    published = _Published([])
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(availability.WsEventPublisher, "publish", published.publish)

    await configure_external_ied_targets(7, [{"ip": "10.10.10.20", "signal_ids": [1]}])
    event = await configure_external_ied_targets(7, [])

    assert event.devices == []
    assert event.removed_signal_ids == [1]
    assert await redis.smembers(availability.TARGET_WORKSPACES_KEY) == set()


@pytest.mark.anyio
async def test_checker_publishes_only_on_status_transition(monkeypatch) -> None:
    redis = _FakeRedis()
    published = _Published([])
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(availability.WsEventPublisher, "publish", published.publish)

    async def fake_reachability(_target, *, timeout_ms: int):
        return VerificationMmsReachabilityResultSchema(
            host="10.10.10.20",
            port=12447,
            reachable=True,
            checked_at="2026-01-01T00:00:00Z",
            error=None,
            check_kind="tcp_connect",
            failure_code=None,
        )

    monkeypatch.setattr(availability, "check_mms_tcp_endpoint", fake_reachability)
    await configure_external_ied_targets(7, [{"ip": "10.10.10.20", "port": 12447, "signal_ids": [1]}])
    published.events.clear()

    stop_event = asyncio.Event()
    task = asyncio.create_task(run_external_ied_availability_checker(stop_event))
    await asyncio.sleep(1.2)
    stop_event.set()
    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        pass

    assert [event.event for event in published.events] == ["external_ied_status_changed"]
    assert published.events[0].old_status == "expected"
    assert published.events[0].new_status == "reachable"
    assert published.events[0].port == 12447


@pytest.mark.anyio
async def test_checker_uses_tcp_probe_and_does_not_call_ping(monkeypatch) -> None:
    redis = _FakeRedis()
    published = _Published([])
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(availability.WsEventPublisher, "publish", published.publish)

    called_targets: list[tuple[str, int]] = []

    async def fake_tcp(target, *, timeout_ms: int):
        called_targets.append((target.host, target.port))
        return VerificationMmsReachabilityResultSchema(
            host=target.host,
            port=target.port,
            reachable=False,
            checked_at="2026-01-01T00:00:00Z",
            error="TCP connection refused",
            check_kind="tcp_connect",
            failure_code="mms_unavailable",
        )

    monkeypatch.setattr(availability, "check_mms_tcp_endpoint", fake_tcp)
    assert not hasattr(availability, "_probe_ping_target")
    await configure_external_ied_targets(7, [{"ip": "10.10.10.20", "port": 12447, "signal_ids": [1]}])
    published.events.clear()

    stop_event = asyncio.Event()
    task = asyncio.create_task(run_external_ied_availability_checker(stop_event))
    await asyncio.sleep(1.2)
    stop_event.set()
    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        pass

    assert called_targets == [("10.10.10.20", 12447)]
    assert published.events[0].new_status == "offline"
    assert published.events[0].failure_code == "mms_unavailable"


@pytest.mark.anyio
async def test_checker_does_not_emit_duplicate_events_for_same_status(monkeypatch) -> None:
    redis = _FakeRedis()
    published = _Published([])
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(availability.WsEventPublisher, "publish", published.publish)

    async def fake_tcp(target, *, timeout_ms: int):
        return VerificationMmsReachabilityResultSchema(
            host=target.host,
            port=target.port,
            reachable=True,
            checked_at="2026-01-01T00:00:00Z",
            error=None,
            check_kind="tcp_connect",
            failure_code=None,
        )

    monkeypatch.setattr(availability, "check_mms_tcp_endpoint", fake_tcp)
    event = await configure_external_ied_targets(7, [{"ip": "10.10.10.20", "signal_ids": [1]}])
    target = availability.ExternalIedTarget(ip="10.10.10.20", port=102, signal_ids=(1,))
    expected = event.devices[0]

    await availability._check_one(7, "10.10.10.20:102", target, expected)
    published.events.clear()
    previous = availability._record_from_payload(
        "10.10.10.20",
        redis.hashes[availability._status_key(7)]["10.10.10.20:102"],
        target,
    )

    await availability._check_one(7, "10.10.10.20:102", target, previous)

    assert published.events == []
