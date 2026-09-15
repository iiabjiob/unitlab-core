from __future__ import annotations

import json
import time
from dataclasses import dataclass
from types import SimpleNamespace
from typing import Any

import pytest

from app.services import external_ied_discovery_scheduler as scheduler
from app.services.external_ied_discovery_scheduler import DiscoveryCacheMetadata, ExternalIedDiscoveryRequest
from app.services.external_ied_discovery_worker import (
    DiscoveryResult,
    DiscoverySummary,
    MmsExternalIedDiscoveryEngine,
    build_discovery_model,
    build_discovery_summary,
    compute_model_fingerprint,
)
from app.workers import external_ied_discovery as worker


class _FakeRedis:
    def __init__(self) -> None:
        self.hashes: dict[str, dict[str, str]] = {}
        self.deleted: list[str] = []
        self.acked: list[tuple[str, str, str]] = []
        self.streams: dict[str, list[dict[str, str]]] = {}

    async def hget(self, key: str, field: str) -> str | None:
        return self.hashes.get(key, {}).get(field)

    async def hset(self, key: str, field: str, value: str) -> None:
        self.hashes.setdefault(key, {})[field] = value

    async def delete(self, *keys: str) -> None:
        self.deleted.extend(keys)

    async def xack(self, stream: str, group: str, entry_id: str) -> None:
        self.acked.append((stream, group, entry_id))

    async def xadd(self, stream: str, fields: dict[str, str], **_kwargs: Any) -> str:
        self.streams.setdefault(stream, []).append(fields)
        return f"{len(self.streams[stream])}-0"


@dataclass
class _Engine:
    non_blocking = True
    result: DiscoveryResult | None = None
    error: Exception | None = None
    requests: list[ExternalIedDiscoveryRequest] | None = None

    def discover(self, request: ExternalIedDiscoveryRequest) -> DiscoveryResult:
        if self.requests is not None:
            self.requests.append(request)
        if self.error is not None:
            raise self.error
        assert self.result is not None
        return self.result


def _request(**overrides: Any) -> ExternalIedDiscoveryRequest:
    values = {
        "request_id": "req-1",
        "workspace_id": 5,
        "endpoint": "172.16.40.128:12447",
        "ip": "172.16.40.128",
        "port": 12447,
        "signal_ids": (1, 2),
        "reason": "verification_required",
        "priority": "high",
        "model_fingerprint": None,
        "planning_fingerprint": "plan-a",
        "requested_at_ms": 100,
        "earliest_execution_at_ms": 100,
        "dedupe": True,
    }
    values.update(overrides)
    return ExternalIedDiscoveryRequest(**values)


def _result(request: ExternalIedDiscoveryRequest) -> DiscoveryResult:
    model = {
        "schema": "unitlab.external-ied.model.v1",
        "ied": "IED_A",
        "datasets": [{"reference": "IED_ALD0/LLN0.ds", "members": ["IED_ALD0/GGIO1.ST.stVal"]}],
        "rcbs": [{"reference": "IED_ALD0/LLN0.BR.brcb01", "name": "brcb01", "dataset_reference": "IED_ALD0/LLN0.ds"}],
        "fcdas": [{"reference": "IED_ALD0/GGIO1.ST.stVal", "fc": "ST"}],
    }
    summary = build_discovery_summary(model, duration_ms=42)
    metadata = DiscoveryCacheMetadata(
        discovery_version="unitlab.external-ied.discovery.v1",
        device_identity="IED_A",
        vendor="UnitLab",
        model="VirtualIED",
        config_rev="7",
        last_discovery_at_ms=1_000,
        last_successful_discovery_at_ms=1_000,
        last_failed_discovery_at_ms=None,
        last_error=None,
        discovery_duration_ms=42,
        model_fingerprint=compute_model_fingerprint(model),
        planning_fingerprint=request.planning_fingerprint,
    )
    return DiscoveryResult(request=request, metadata=metadata, summary=summary, model=model)


@pytest.mark.anyio
async def test_execute_discovery_request_records_result_without_ui_or_retry(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    monkeypatch.setattr(scheduler.RedisManager, "get_instance", lambda: redis)
    request = _request()
    engine = _Engine(result=_result(request), requests=[])

    await worker.execute_discovery_request(request, engine=engine)

    assert engine.requests == [request]
    state_payload = json.loads(redis.hashes["external_ied:workspace:5:discovery_state"][request.endpoint])
    cache_payload = json.loads(redis.hashes["external_ied:workspace:5:discovery_cache"][request.endpoint])
    model_payload = json.loads(redis.hashes["external_ied:workspace:5:discovery_model"][request.endpoint])

    assert state_payload["state"] == "Succeeded"
    assert state_payload["ready_for_verification"] is True
    assert cache_payload["model_fingerprint"] == compute_model_fingerprint(model_payload)
    assert cache_payload["planning_fingerprint"] == "plan-a"
    event_payload = json.loads(redis.streams["external-ied-planning:events"][0]["data"])
    assert event_payload["event"] == "ExternalIedDiscoveryCompleted"
    assert event_payload["endpoint"] == request.endpoint
    assert event_payload["signal_ids"] == [1, 2]
    assert "external_ied:workspace:5:discovery_pending:172.16.40.128:12447" in redis.deleted


@pytest.mark.anyio
async def test_execute_discovery_request_records_permanent_failure_without_retry(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    monkeypatch.setattr(scheduler.RedisManager, "get_instance", lambda: redis)
    request = _request()
    engine = _Engine(error=RuntimeError("invalid discovery model"), requests=[])

    with pytest.raises(RuntimeError):
        await worker.execute_discovery_request(request, engine=engine)

    state_payload = json.loads(redis.hashes["external_ied:workspace:5:discovery_state"][request.endpoint])
    cache_payload = json.loads(redis.hashes["external_ied:workspace:5:discovery_cache"][request.endpoint])

    assert state_payload["state"] == "Failed"
    assert state_payload["last_error"] == "invalid discovery model"
    assert cache_payload["last_error"] == "invalid discovery model"
    assert cache_payload["last_failed_discovery_at_ms"] is not None
    assert state_payload["retry_at_ms"] is None


@pytest.mark.anyio
async def test_execute_discovery_request_records_retry_for_transient_failure(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    monkeypatch.setattr(scheduler.RedisManager, "get_instance", lambda: redis)
    request = _request()
    engine = _Engine(error=RuntimeError("TCP connect timeout"), requests=[])

    with pytest.raises(RuntimeError):
        await worker.execute_discovery_request(request, engine=engine)

    state_payload = json.loads(redis.hashes["external_ied:workspace:5:discovery_state"][request.endpoint])
    cache_payload = json.loads(redis.hashes["external_ied:workspace:5:discovery_cache"][request.endpoint])

    assert state_payload["state"] == "RetryWaiting"
    assert state_payload["retry_at_ms"] > state_payload["failed_at_ms"]
    assert state_payload["retry_count"] == 1
    assert cache_payload["last_error"] == "TCP connect timeout"


@pytest.mark.anyio
async def test_process_entries_acks_failed_request_without_scheduling_retry(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    monkeypatch.setattr(scheduler.RedisManager, "get_instance", lambda: redis)
    request = _request()
    fields = {"data": json.dumps(request.to_payload())}

    await worker._process_entries(redis, [("1-0", fields)], engine=_Engine(error=RuntimeError("timeout"), requests=[]))

    assert redis.acked == [(worker.STREAM_NAME, worker.GROUP_NAME, "1-0")]
    state_payload = json.loads(redis.hashes["external_ied:workspace:5:discovery_state"][request.endpoint])
    assert state_payload["state"] == "RetryWaiting"
    assert state_payload["retry_at_ms"] is not None


@pytest.mark.anyio
async def test_worker_respects_earliest_execution_at_ms(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    monkeypatch.setattr(scheduler.RedisManager, "get_instance", lambda: redis)
    request = _request(earliest_execution_at_ms=2_000)
    fields = {"data": json.dumps(request.to_payload())}
    sleeps: list[float] = []
    times = iter([1.0, 2.0, 2.0])
    monkeypatch.setattr(
        worker,
        "time",
        SimpleNamespace(time=lambda: next(times), monotonic=time.monotonic),
    )

    async def _sleep(delay: float) -> None:
        sleeps.append(delay)

    monkeypatch.setattr(worker.asyncio, "sleep", _sleep)

    await worker._process_entries(redis, [("1-0", fields)], engine=_Engine(result=_result(request), requests=[]))

    assert sleeps == [1.0]
    assert redis.acked == [(worker.STREAM_NAME, worker.GROUP_NAME, "1-0")]


@pytest.mark.anyio
async def test_wait_until_due_uses_short_cancellable_chunks(monkeypatch: pytest.MonkeyPatch) -> None:
    request = _request(earliest_execution_at_ms=10_000)
    sleeps: list[float] = []
    values = iter([1.0, 2.0, 10.0])
    monkeypatch.setattr(worker.time, "time", lambda: next(values))

    async def _sleep(delay: float) -> None:
        sleeps.append(delay)

    monkeypatch.setattr(worker.asyncio, "sleep", _sleep)

    await worker._wait_until_due(request, max_sleep_seconds=1.0)

    assert sleeps == [1.0, 1.0]


@pytest.mark.anyio
async def test_wait_until_due_cancels_when_stop_event_is_set() -> None:
    stop_event = worker.asyncio.Event()
    stop_event.set()

    with pytest.raises(worker.asyncio.CancelledError):
        await worker._wait_until_due(_request(earliest_execution_at_ms=10_000), stop_event=stop_event)


@pytest.mark.anyio
async def test_run_discovery_wraps_blocking_engine_with_to_thread(monkeypatch: pytest.MonkeyPatch) -> None:
    request = _request()
    result = _result(request)
    calls: list[tuple[object, ExternalIedDiscoveryRequest]] = []

    class _BlockingEngine:
        def discover(self, value: ExternalIedDiscoveryRequest) -> DiscoveryResult:
            return result

    async def _to_thread(func: object, value: ExternalIedDiscoveryRequest) -> DiscoveryResult:
        calls.append((func, value))
        return result

    monkeypatch.setattr(worker.asyncio, "to_thread", _to_thread)

    assert await worker._run_discovery(_BlockingEngine(), request) is result
    assert calls and calls[0][1] is request


@pytest.mark.anyio
async def test_stale_upgraded_stream_entry_is_rejected_before_discovery(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    request = _request(request_id="old-normal", reason="cache_missing", priority="normal")
    state = scheduler.DiscoveryStateMachine()
    state.markQueued(now_ms=100, request_id="new-high", priority="high", reason="verification_required")
    redis.hashes["external_ied:workspace:5:discovery_state"] = {
        request.endpoint: json.dumps(state.to_payload(), separators=(",", ":")),
    }
    monkeypatch.setattr(scheduler.RedisManager, "get_instance", lambda: redis)
    engine = _Engine(result=_result(request), requests=[])

    await worker._process_entries(redis, [("1-0", {"data": json.dumps(request.to_payload())})], engine=engine)

    assert engine.requests == []
    assert redis.acked == [(worker.STREAM_NAME, worker.GROUP_NAME, "1-0")]


@pytest.mark.anyio
async def test_completed_event_is_not_emitted_when_cache_persistence_fails(monkeypatch: pytest.MonkeyPatch) -> None:
    class _FailingRedis(_FakeRedis):
        async def hset(self, key: str, field: str, value: str) -> None:
            if key.endswith(":discovery_model"):
                raise RuntimeError("redis write failed")
            await super().hset(key, field, value)

    redis = _FailingRedis()
    monkeypatch.setattr(scheduler.RedisManager, "get_instance", lambda: redis)
    request = _request()

    with pytest.raises(RuntimeError):
        await worker.execute_discovery_request(request, engine=_Engine(result=_result(request), requests=[]))

    assert redis.streams == {}


def test_model_fingerprint_ignores_endpoint_and_signal_list_configuration() -> None:
    discovery = {
        "iedName": "IED_A",
        "endpoint": {"host": "172.16.40.128", "port": 12447},
        "dataSets": [{"reference": "IED_ALD0/LLN0.ds", "members": [{"reference": "IED_ALD0/GGIO1.ST.stVal", "fc": "ST"}]}],
        "reportControls": [{"id": "IED_ALD0/LLN0.BR.brcb01", "name": "brcb01", "dataSetRef": "IED_ALD0/LLN0.ds"}],
    }
    same_model_different_endpoint = {
        **discovery,
        "endpoint": {"host": "10.20.30.40", "port": 102},
    }

    model_a = build_discovery_model(discovery)
    model_b = build_discovery_model(same_model_different_endpoint)

    assert compute_model_fingerprint(model_a) == compute_model_fingerprint(model_b)


def test_model_fingerprint_ignores_discovery_diagnostics() -> None:
    discovery = {
        "iedName": "IED_A",
        "dataSets": [{"reference": "IED_ALD0/LLN0.ds", "members": [{"reference": "IED_ALD0/GGIO1.ST.stVal", "fc": "ST"}]}],
        "reportControls": [{"id": "IED_ALD0/LLN0.BR.brcb01", "name": "brcb01", "dataSetRef": "IED_ALD0/LLN0.ds"}],
    }
    model = build_discovery_model(discovery)
    model_with_diagnostics = {**model, "diagnostics": ["native-wire-client: discover-skip=dataset-members domain=IED_A reason=decode-failed"]}

    assert compute_model_fingerprint(model) == compute_model_fingerprint(model_with_diagnostics)


def test_build_discovery_summary_counts_model_elements() -> None:
    model = build_discovery_model(
        {
            "iedName": "IED_A",
            "dataSets": [{"reference": "IED_ALD0/LLN0.ds", "members": [{"reference": "IED_ALD0/GGIO1.ST.stVal", "fc": "ST"}]}],
            "reportControls": [{"id": "IED_ALD0/LLN0.BR.brcb01", "name": "brcb01", "dataSetRef": "IED_ALD0/LLN0.ds"}],
        }
    )
    summary = build_discovery_summary(model, duration_ms=25)

    assert summary == DiscoverySummary(
        logical_devices=1,
        logical_nodes=1,
        datasets=1,
        dataset_members=1,
        report_controls=1,
        signals=1,
        duration_ms=25,
    )


def test_mms_engine_configures_external_mms_target_and_closes_session() -> None:
    calls: dict[str, Any] = {"order": []}

    class _Snapshot:
        last_discovery = {
            "iedName": "IED_A",
            "vendor": "UnitLab",
            "model": "VirtualIED",
            "configRev": "7",
            "dataSets": [{"reference": "IED_ALD0/LLN0.ds", "members": [{"reference": "IED_ALD0/GGIO1.ST.stVal", "fc": "ST"}]}],
            "reportControls": [{"id": "IED_ALD0/LLN0.BR.brcb01", "name": "brcb01", "dataSetRef": "IED_ALD0/LLN0.ds"}],
        }

    class _ControlService:
        def __init__(self, *, client_id: str) -> None:
            calls["client_id"] = client_id

        def configure_target(self, request: Any) -> None:
            calls["target"] = request
            calls["order"].append("configure")

        def connect_ied(self) -> None:
            calls["connect"] = True
            calls["order"].append("connect")

        def discover_ied(self) -> _Snapshot:
            calls["discover"] = True
            calls["order"].append("discover")
            return _Snapshot()

        def close_ied(self) -> None:
            calls["closed"] = True
            calls["order"].append("close")

    request = _request(ip="172.16.40.128", port=12447, endpoint="172.16.40.128:12447")
    result = MmsExternalIedDiscoveryEngine(control_service_factory=_ControlService).discover(request)

    assert calls["target"].mode == "external-mms"
    assert calls["target"].host == "172.16.40.128"
    assert calls["target"].port == 12447
    assert calls["connect"] is True
    assert calls["discover"] is True
    assert calls["closed"] is True
    assert calls["order"] == ["configure", "connect", "discover", "close"]
    assert result.metadata.model_fingerprint == compute_model_fingerprint(result.model)
    assert result.metadata.planning_fingerprint == "plan-a"
