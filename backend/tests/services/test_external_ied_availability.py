from __future__ import annotations

import asyncio
import json
from dataclasses import dataclass
from typing import cast

import pytest

from app.schemas.verification_schema import (
    VerificationMmsReachabilityResultSchema,
    VerificationMmsReachabilityTargetSchema,
)
from app.schemas.ws.events import ExternalIedStatus, ExternalIedStatusChangedEvent
from app.services import external_ied_availability as availability
from app.services.external_ied_availability import configure_external_ied_targets, run_external_ied_availability_checker


class _FakePipeline:
    def __init__(self, redis: "_FakeRedis") -> None:
        self.redis: _FakeRedis = redis
        self.ops: list[tuple[str, tuple[str, ...]]] = []

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

    async def hget(self, key: str, field: str) -> str | None:
        return self.hashes.get(key, {}).get(field)

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
            _ = self.hashes.pop(key, None)
            _ = self.sets.pop(key, None)
            _ = self.values.pop(key, None)

    async def set(self, key: str, value: str, **_kwargs: object) -> bool:
        if _kwargs.get("nx") and key in self.values:
            return False
        self.values[key] = value
        return True

    async def get(self, key: str) -> str | None:
        return self.values.get(key)

    async def xadd(self, stream: str, fields: dict[str, object], **_kwargs: object) -> str:
        _ = fields
        _ = self.values.setdefault(f"xadd:{stream}", "0")
        current = int(self.values[f"xadd:{stream}"]) + 1
        self.values[f"xadd:{stream}"] = str(current)
        return f"{current}-0"

    def pipeline(self) -> _FakePipeline:
        return _FakePipeline(self)


@dataclass
class _Published:
    events: list[object]

    async def publish(self, event: object) -> None:
        self.events.append(event)


def _state(
    endpoint: str = "10.10.10.20:102",
    *,
    status: ExternalIedStatus = "expected",
    next_probe_at_ms: int = 0,
    last_probe_at_ms: int | None = None,
    consecutive_successes: int = 0,
    consecutive_failures: int = 0,
    priority_reason: str | None = None,
) -> availability.EndpointProbeState:
    host, raw_port = endpoint.rsplit(":", 1)
    config = availability.ExternalIedWatcherConfig()
    return availability.EndpointProbeState(
        endpoint_id=f"7:{endpoint}",
        workspace_id=7,
        host=host,
        port=int(raw_port),
        signal_ids=(1,),
        status=status,
        last_probe_at_ms=last_probe_at_ms,
        next_probe_at_ms=next_probe_at_ms,
        last_success_at_ms=None,
        last_failure_at_ms=None,
        consecutive_successes=consecutive_successes,
        consecutive_failures=consecutive_failures,
        active_probe=False,
        priority_reason=priority_reason or availability._priority_reason_for_record(  # pyright: ignore[reportPrivateUsage]
            status,
            consecutive_successes,
            consecutive_failures,
            config,
        ),
    )


def _probe_result(*, reachable: bool) -> VerificationMmsReachabilityResultSchema:
    return VerificationMmsReachabilityResultSchema(
        host="10.10.10.20",
        port=102,
        reachable=reachable,
        checked_at="2026-01-01T00:00:00Z",
        error=None if reachable else "TCP connect timeout",
        check_kind="tcp_connect",
        failure_code=None if reachable else "unreachable",
    )


@pytest.mark.anyio
async def test_configure_external_ied_targets_writes_expected_snapshot(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    published = _Published([])
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(availability.WsEventPublisher, "publish", published.publish)  # pyright: ignore[reportPrivateLocalImportUsage]

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
async def test_configure_external_ied_targets_clears_backend_context(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    published = _Published([])
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(availability.WsEventPublisher, "publish", published.publish)  # pyright: ignore[reportPrivateLocalImportUsage]

    _ = await configure_external_ied_targets(7, [{"ip": "10.10.10.20", "signal_ids": [1]}])
    event = await configure_external_ied_targets(7, [])

    assert event.devices == []
    assert event.removed_signal_ids == [1]
    assert await redis.smembers(availability.TARGET_WORKSPACES_KEY) == set()


@pytest.mark.anyio
async def test_load_external_ied_discovery_tree_reads_cached_model(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    await redis.hset(
        availability._discovery_model_key(7),  # pyright: ignore[reportPrivateUsage]
        "10.10.10.20:102",
        json.dumps({
            "datasets": [{"reference": "IEDLD0/LLN0.ds", "members": ["IEDLD0/GGIO1.ST.stVal"]}],
            "rcbs": [{"reference": "IEDLD0/LLN0.BR.brcb01", "name": "brcb01", "kind": "buffered", "dataset_reference": "IEDLD0/LLN0.ds"}],
            "fcdas": [{"reference": "IEDLD0/GGIO1.ST.stVal", "fc": "ST"}],
        }),
    )
    await redis.hset(
        availability._cache_key(7),  # pyright: ignore[reportPrivateUsage]
        "10.10.10.20:102",
        json.dumps({"model_fingerprint": "model-1"}),
    )

    tree = await availability.load_external_ied_discovery_tree(7, "10.10.10.20:102")

    assert tree is not None
    assert tree["endpoint"] == "10.10.10.20:102"
    assert tree["model_fingerprint"] == "model-1"
    reports = cast(list[dict[str, object]], tree["reports"])
    report = reports[0]
    dataset = cast(dict[str, object], report["dataset"])
    signals = cast(list[dict[str, object]], dataset["signals"])
    signal = signals[0]
    assert report["name"] == "brcb01"
    assert dataset["reference"] == "IEDLD0/LLN0.ds"
    assert signal["reference"] == "IEDLD0/GGIO1.ST.stVal"
    assert signal["fc"] == "ST"


@pytest.mark.anyio
async def test_status_snapshot_does_not_include_discovery_model_tree(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    await redis.hset(
        availability._status_key(7),  # pyright: ignore[reportPrivateUsage]
        "10.10.10.20:102",
        json.dumps({
            "ip": "10.10.10.20",
            "port": 102,
            "status": "reachable",
            "signal_ids": [1],
            "check_kind": "tcp_connect",
        }),
    )
    await redis.hset(
        availability._discovery_model_key(7),  # pyright: ignore[reportPrivateUsage]
        "10.10.10.20:102",
        json.dumps({"datasets": [], "rcbs": [], "fcdas": []}),
    )

    event = await availability.build_external_ied_status_snapshot(7)

    assert not hasattr(event.devices[0], "discovery_tree")


@pytest.mark.anyio
async def test_checker_publishes_only_on_status_transition(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    published = _Published([])
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(availability.WsEventPublisher, "publish", published.publish)  # pyright: ignore[reportPrivateLocalImportUsage]

    async def fake_reachability(
        _target: VerificationMmsReachabilityTargetSchema, *, timeout_ms: int
    ) -> VerificationMmsReachabilityResultSchema:
        _ = timeout_ms
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
    _ = await configure_external_ied_targets(7, [{"ip": "10.10.10.20", "port": 12447, "signal_ids": [1]}])
    _ = published.events.clear()

    stop_event = asyncio.Event()
    task = asyncio.create_task(run_external_ied_availability_checker(stop_event))
    await asyncio.sleep(1.2)
    stop_event.set()
    _ = task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        pass

    events = [cast(ExternalIedStatusChangedEvent, event) for event in published.events]
    assert [event.event for event in events] == ["external_ied_status_changed"]
    assert events[0].old_status == "expected"
    assert events[0].new_status == "reachable"
    assert events[0].port == 12447


@pytest.mark.anyio
async def test_checker_uses_tcp_probe_and_does_not_call_ping(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    published = _Published([])
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(availability.WsEventPublisher, "publish", published.publish)  # pyright: ignore[reportPrivateLocalImportUsage]

    called_targets: list[tuple[str, int]] = []

    async def fake_tcp(target: VerificationMmsReachabilityTargetSchema, *, timeout_ms: int):
        _ = timeout_ms
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
    _ = await configure_external_ied_targets(7, [{"ip": "10.10.10.20", "port": 12447, "signal_ids": [1]}])
    _ = published.events.clear()

    stop_event = asyncio.Event()
    task = asyncio.create_task(run_external_ied_availability_checker(stop_event))
    await asyncio.sleep(1.2)
    stop_event.set()
    _ = task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        pass

    assert called_targets == [("10.10.10.20", 12447)]
    event = cast(ExternalIedStatusChangedEvent, published.events[0])
    assert event.new_status == "offline"
    assert event.failure_code == "mms_unavailable"


@pytest.mark.anyio
async def test_checker_does_not_emit_duplicate_events_for_same_status(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    published = _Published([])
    monkeypatch.setattr(availability.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(availability.WsEventPublisher, "publish", published.publish)  # pyright: ignore[reportPrivateLocalImportUsage]

    async def fake_tcp(target: VerificationMmsReachabilityTargetSchema, *, timeout_ms: int):
        _ = timeout_ms
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

    await availability._check_one(7, "10.10.10.20:102", target, expected)  # pyright: ignore[reportPrivateUsage]
    _ = published.events.clear()
    previous = availability._record_from_payload(  # pyright: ignore[reportPrivateUsage]
        "10.10.10.20",
        redis.hashes[availability._status_key(7)]["10.10.10.20:102"],  # pyright: ignore[reportPrivateUsage]
        target,
    )

    await availability._check_one(7, "10.10.10.20:102", target, previous)  # pyright: ignore[reportPrivateUsage]

    assert published.events == []


def test_scheduler_checks_expected_endpoints_first_pass_quickly() -> None:
    scheduler = availability.ExternalIedPriorityScheduler()
    scheduler.set_targets([_state("10.10.10.20:102", status="expected")], now_ms=100)

    due = scheduler.pop_due(now_ms=100, max_count=1)

    assert [state.endpoint_id for state in due] == ["7:10.10.10.20:102"]


def test_reachable_candidate_is_probed_more_frequently_than_stable() -> None:
    config = availability.ExternalIedWatcherConfig(
        reachable_candidate_interval_ms=2_000,
        reachable_stable_interval_ms=30_000,
        interval_jitter_ratio=0,
    )
    candidate = _state("10.10.10.20:102", status="expected")
    first = availability.resolve_probe_transition(candidate, _probe_result(reachable=True), now_ms=1_000, config=config)
    first_reason = first.state.priority_reason
    first_next_check = first.next_check_at_ms
    second = availability.resolve_probe_transition(first.state, _probe_result(reachable=True), now_ms=2_000, config=config)

    assert first_reason == availability.PRIORITY_REASON_REACHABLE_CANDIDATE
    assert first_next_check == 3_000
    assert second.state.priority_reason == availability.PRIORITY_REASON_REACHABLE_STABLE
    assert second.next_check_at_ms == 32_000


def test_stable_reachable_is_not_marked_offline_after_one_failed_probe() -> None:
    config = availability.ExternalIedWatcherConfig(offline_failure_threshold=2, suspect_offline_interval_ms=1_500, interval_jitter_ratio=0)
    stable = _state(
        "10.10.10.20:102",
        status="reachable",
        consecutive_successes=2,
        priority_reason=availability.PRIORITY_REASON_REACHABLE_STABLE,
    )

    transition = availability.resolve_probe_transition(stable, _probe_result(reachable=False), now_ms=10_000, config=config)

    assert transition.old_status == "reachable"
    assert transition.new_status == "reachable"
    assert transition.status_changed is False
    assert transition.state.priority_reason == availability.PRIORITY_REASON_SUSPECT_OFFLINE
    assert transition.next_check_at_ms == 11_500


def test_suspect_offline_becomes_offline_after_second_failure() -> None:
    config = availability.ExternalIedWatcherConfig(offline_failure_threshold=2)
    suspect = _state(
        "10.10.10.20:102",
        status="reachable",
        consecutive_failures=1,
        priority_reason=availability.PRIORITY_REASON_SUSPECT_OFFLINE,
    )

    transition = availability.resolve_probe_transition(suspect, _probe_result(reachable=False), now_ms=10_000, config=config)

    assert transition.old_status == "reachable"
    assert transition.new_status == "offline"
    assert transition.status_changed is True
    assert transition.state.priority_reason == availability.PRIORITY_REASON_OFFLINE


def test_offline_endpoint_is_retried_periodically_not_too_aggressively() -> None:
    config = availability.ExternalIedWatcherConfig(offline_interval_ms=7_500, interval_jitter_ratio=0)
    offline = _state("10.10.10.20:102", status="offline", consecutive_failures=3)

    transition = availability.resolve_probe_transition(offline, _probe_result(reachable=False), now_ms=10_000, config=config)

    assert transition.new_status == "offline"
    assert transition.next_check_at_ms == 17_500


def test_verification_needed_endpoint_is_probed_immediately() -> None:
    scheduler = availability.ExternalIedPriorityScheduler(availability.ExternalIedWatcherConfig(cold_start_duration_ms=0))
    endpoint_id = "7:10.10.10.20:102"
    scheduler.set_targets([_state("10.10.10.20:102", status="reachable", next_probe_at_ms=60_000)], now_ms=1_000)

    scheduler.markVerificationNeeded([endpoint_id], now_ms=1_000)

    due = scheduler.pop_due(now_ms=1_000, max_count=1)
    assert [state.endpoint_id for state in due] == [endpoint_id]
    assert due[0].priority_reason == availability.PRIORITY_REASON_VERIFICATION_NEEDED


def test_scheduler_respects_max_concurrency_and_active_probe_guard() -> None:
    scheduler = availability.ExternalIedPriorityScheduler()
    scheduler.set_targets([
        _state("10.10.10.20:102", status="expected"),
        _state("10.10.10.21:102", status="expected"),
        _state("10.10.10.22:102", status="expected"),
    ], now_ms=0)

    first = scheduler.pop_due(now_ms=0, max_count=2)
    second = scheduler.pop_due(now_ms=0, max_count=2)

    assert len(first) == 2
    assert len(second) == 1
    assert not set(state.endpoint_id for state in first).intersection(state.endpoint_id for state in second)


def test_scheduler_ignores_stale_queue_entries_after_priority_update() -> None:
    scheduler = availability.ExternalIedPriorityScheduler(availability.ExternalIedWatcherConfig(cold_start_duration_ms=0))
    endpoint_id = "7:10.10.10.20:102"
    scheduler.set_targets([_state("10.10.10.20:102", status="reachable", next_probe_at_ms=60_000)], now_ms=0)
    scheduler.boostEndpoint(endpoint_id, now_ms=1_000)

    due = scheduler.pop_due(now_ms=1_000, max_count=2)
    assert [state.endpoint_id for state in due] == [endpoint_id]
    assert scheduler.pop_due(now_ms=60_000, max_count=2) == []


def test_due_low_priority_endpoint_is_not_starved_by_future_high_priority() -> None:
    scheduler = availability.ExternalIedPriorityScheduler(availability.ExternalIedWatcherConfig(cold_start_duration_ms=0))
    scheduler.set_targets([
        _state("10.10.10.20:102", status="reachable", next_probe_at_ms=10_000, priority_reason=availability.PRIORITY_REASON_REACHABLE_STABLE),
        _state("10.10.10.21:102", status="expected", next_probe_at_ms=60_000),
    ], now_ms=0)

    due = scheduler.pop_due(now_ms=10_000, max_count=1)

    assert [state.endpoint_id for state in due] == ["7:10.10.10.20:102"]


def test_ageing_improves_effective_priority_for_long_overdue_endpoint() -> None:
    config = availability.ExternalIedWatcherConfig(
        ageing_step_ms=1_000,
        max_ageing_priority_boost=3,
        cold_start_duration_ms=0,
    )
    scheduler = availability.ExternalIedPriorityScheduler(config)
    scheduler.set_targets([
        _state("10.10.10.20:102", status="reachable", next_probe_at_ms=0, priority_reason=availability.PRIORITY_REASON_REACHABLE_STABLE),
        _state("10.10.10.21:102", status="offline", next_probe_at_ms=3_000, priority_reason=availability.PRIORITY_REASON_OFFLINE),
    ], now_ms=0)

    due = scheduler.pop_due(now_ms=3_000, max_count=1)

    assert [state.endpoint_id for state in due] == ["7:10.10.10.20:102"]


def test_verification_needed_auto_expires_after_success() -> None:
    config = availability.ExternalIedWatcherConfig(
        verification_needed_interval_ms=1_000,
        reachable_candidate_interval_ms=2_000,
        interval_jitter_ratio=0,
        cold_start_duration_ms=0,
    )
    scheduler = availability.ExternalIedPriorityScheduler(config)
    endpoint_id = "7:10.10.10.20:102"
    state = _state("10.10.10.20:102", status="expected", next_probe_at_ms=60_000)
    scheduler.set_targets([state], now_ms=1_000)
    scheduler.markVerificationNeeded([endpoint_id], now_ms=1_000)
    due = scheduler.pop_due(now_ms=1_000, max_count=1)[0]
    transition = availability.resolve_probe_transition(due, _probe_result(reachable=True), now_ms=1_100, config=config)

    scheduler.complete_probe(endpoint_id, transition)

    assert scheduler.states[endpoint_id].priority_reason == availability.PRIORITY_REASON_REACHABLE_CANDIDATE
    assert scheduler.states[endpoint_id].next_probe_at_ms == 3_100


def test_verification_needed_expires_after_timeout_without_manual_clear() -> None:
    config = availability.ExternalIedWatcherConfig(
        verification_needed_timeout_ms=1_000,
        interval_jitter_ratio=0,
        cold_start_duration_ms=0,
    )
    scheduler = availability.ExternalIedPriorityScheduler(config)
    endpoint_id = "7:10.10.10.20:102"
    scheduler.set_targets([_state("10.10.10.20:102", status="offline", next_probe_at_ms=60_000)], now_ms=0)
    scheduler.markVerificationNeeded([endpoint_id], now_ms=0)

    assert scheduler.pop_due(now_ms=1_001, max_count=1) == []
    assert scheduler.states[endpoint_id].priority_reason == availability.PRIORITY_REASON_OFFLINE


def test_polling_interval_jitter_is_configurable(monkeypatch: pytest.MonkeyPatch) -> None:
    def _uniform(_low: float, _high: float) -> float:
        return 1.2

    monkeypatch.setattr(availability.random, "uniform", _uniform)  # pyright: ignore[reportPrivateLocalImportUsage]
    config = availability.ExternalIedWatcherConfig(
        reachable_candidate_interval_ms=2_000,
        interval_jitter_ratio=0.2,
    )
    candidate = _state("10.10.10.20:102", status="expected")

    transition = availability.resolve_probe_transition(candidate, _probe_result(reachable=True), now_ms=1_000, config=config)

    assert transition.next_check_at_ms == 3_400


def test_repeated_same_status_does_not_emit_status_change() -> None:
    config = availability.ExternalIedWatcherConfig()
    offline = _state("10.10.10.20:102", status="offline", consecutive_failures=1)

    transition = availability.resolve_probe_transition(offline, _probe_result(reachable=False), now_ms=10_000, config=config)

    assert transition.old_status == "offline"
    assert transition.new_status == "offline"
    assert transition.status_changed is False
