from __future__ import annotations

from dataclasses import dataclass, field

import pytest

from app.services import external_ied_discovery_scheduler as scheduler


@dataclass
class _Sink:
    requests: list[scheduler.ExternalIedDiscoveryRequest] = field(default_factory=list)

    async def enqueue(self, request: scheduler.ExternalIedDiscoveryRequest) -> str:
        self.requests.append(request)
        return f"{len(self.requests)}-0"


class _Redis:
    def __init__(self) -> None:
        self.values: dict[str, str] = {}
        self.hashes: dict[str, dict[str, str]] = {}
        self.streams: dict[str, list[dict[str, str]]] = {}

    async def set(self, key: str, value: str, **kwargs) -> bool:
        if kwargs.get("nx") and key in self.values:
            return False
        self.values[key] = value
        return True

    async def get(self, key: str) -> str | None:
        return self.values.get(key)

    async def delete(self, *keys: str) -> None:
        for key in keys:
            self.values.pop(key, None)

    async def xadd(self, stream: str, fields: dict[str, str], **_kwargs) -> str:
        self.streams.setdefault(stream, []).append(fields)
        return f"{len(self.streams[stream])}-0"

    async def hset(self, key: str, field: str, value: str) -> None:
        self.hashes.setdefault(key, {})[field] = value


def _endpoint(
    *,
    status: str = "reachable",
    priority_reason: str | None = scheduler.REACHABLE_STABLE_REASON,
    failure_code: str | None = None,
) -> scheduler.ExternalIedDiscoveryEndpointState:
    return scheduler.ExternalIedDiscoveryEndpointState(
        workspace_id=5,
        ip="172.16.40.128",
        port=12447,
        status=status,
        signal_ids=(1, 2),
        priority_reason=priority_reason,
        failure_code=failure_code,
        last_checked_at="2026-07-04T08:27:09Z",
    )


def _service(sink: _Sink | None = None) -> scheduler.ExternalIedDiscoveryScheduler:
    return scheduler.ExternalIedDiscoveryScheduler(sink=sink or _Sink(), now_ms=lambda: 123_000)


def _metadata(
    *,
    planning_fingerprint: str = "plan-a",
    model_fingerprint: str = "model-a",
) -> scheduler.DiscoveryCacheMetadata:
    return scheduler.DiscoveryCacheMetadata(
        discovery_version="unitlab-discovery.v1",
        device_identity="IED-A",
        vendor="UnitLab",
        model="VirtualIED",
        config_rev="42",
        last_discovery_at_ms=100,
        last_successful_discovery_at_ms=100,
        last_failed_discovery_at_ms=None,
        last_error=None,
        discovery_duration_ms=25,
        model_fingerprint=model_fingerprint,
        planning_fingerprint=planning_fingerprint,
    )


def test_policy_reachable_stable_with_valid_cache_is_no_action() -> None:
    decision = scheduler.DiscoveryPolicy().evaluate(
        endpoint_state=_endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON),
        cache_metadata=_metadata(),
        now_ms=123_000,
    )

    assert decision.decision == "NoAction"


def test_policy_cache_missing_with_reachable_stable_queues_discovery() -> None:
    decision = scheduler.DiscoveryPolicy().evaluate(
        endpoint_state=_endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON),
        now_ms=123_000,
    )

    assert decision.decision == "QueueDiscovery"
    assert decision.reason == "cache_missing"


def test_discovery_state_machine_transitions_and_verification_readiness() -> None:
    state = scheduler.DiscoveryStateMachine()

    assert state.canSchedule(now_ms=100)
    state.markQueued(now_ms=100)
    assert not state.canSchedule(now_ms=101)
    state.markRunning(now_ms=110)
    assert not state.canSchedule(now_ms=111)
    state.markCompleted(now_ms=120)

    assert state.state == "Succeeded"
    assert state.isReadyForVerification()
    assert state.canSchedule(now_ms=121)


def test_discovery_state_machine_retry_waiting_becomes_schedulable_after_timer() -> None:
    state = scheduler.DiscoveryStateMachine()
    state.markQueued(now_ms=100, request_id="req", priority="high", reason="verification_required")
    state.markRunning(now_ms=110)
    state.markRetry(now_ms=120, retry_at_ms=500, error="temporary")

    assert state.state == "RetryWaiting"
    assert not state.canSchedule(now_ms=499)
    assert state.canSchedule(now_ms=500)
    assert state.queued_priority == "high"
    assert state.queued_reason == "verification_required"


def test_discovery_state_machine_rejects_implicit_running_transition() -> None:
    state = scheduler.DiscoveryStateMachine()

    with pytest.raises(ValueError):
        state.markRunning(now_ms=100)


def test_policy_waits_for_candidate_without_pending_verification() -> None:
    endpoint = _endpoint(priority_reason=scheduler.REACHABLE_CANDIDATE_REASON)
    decision = scheduler.DiscoveryPolicy().evaluate(
        endpoint_state=endpoint,
        cache_metadata=_metadata(),
        now_ms=123_000,
    )

    assert decision.decision == "WaitForStableConnection"


def test_policy_blocks_queued_running_and_retry_waiting_before_timer() -> None:
    queued = scheduler.DiscoveryStateMachine()
    queued.markQueued(now_ms=100)
    running = scheduler.DiscoveryStateMachine()
    running.markQueued(now_ms=100)
    running.markRunning(now_ms=110)
    retry = scheduler.DiscoveryStateMachine()
    retry.markQueued(now_ms=100)
    retry.markRunning(now_ms=110)
    retry.markRetry(now_ms=120, retry_at_ms=500)

    for lifecycle in (queued, running, retry):
        decision = scheduler.DiscoveryPolicy().evaluate(
            endpoint_state=_endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON),
            previous_discovery_state=scheduler.PreviousDiscoveryState(lifecycle=lifecycle),
            now_ms=200,
        )
        assert decision.decision == "NoAction"


def test_policy_queues_high_priority_for_verification_required() -> None:
    decision = scheduler.DiscoveryPolicy().evaluate(
        endpoint_state=_endpoint(priority_reason=scheduler.REACHABLE_CANDIDATE_REASON),
        cache_metadata=_metadata(model_fingerprint="actual-model"),
        verification_context=scheduler.VerificationContext(verification_required=True, planning_fingerprint="plan-b"),
        now_ms=123_000,
    )

    assert decision.decision == "QueueHighPriorityDiscovery"
    assert decision.reason == "verification_required"
    assert decision.priority == "high"
    assert decision.model_fingerprint == "actual-model"
    assert decision.planning_fingerprint == "plan-b"


def test_policy_ignores_not_applicable_endpoint() -> None:
    decision = scheduler.DiscoveryPolicy().evaluate(
        endpoint_state=_endpoint(status="not_applicable", priority_reason=None),
        user_action=scheduler.UserAction(action="refresh"),
        now_ms=123_000,
    )

    assert decision.decision == "Ignore"


@pytest.mark.anyio
async def test_schedules_when_endpoint_becomes_reachable_stable() -> None:
    sink = _Sink()
    service = _service(sink)

    request = await service.on_watcher_state(_endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON))

    assert request is not None
    assert request.reason == "cache_missing"
    assert sink.requests == [request]


@pytest.mark.anyio
async def test_schedules_reachable_candidate_only_when_verification_pending_or_cache_needed() -> None:
    sink = _Sink()
    service = _service(sink)
    endpoint = _endpoint(priority_reason=scheduler.REACHABLE_CANDIDATE_REASON)
    fresh_cache = _metadata()

    assert await service.on_watcher_state(endpoint, cache_record=fresh_cache, verification_pending=False) is None

    request = await service.on_watcher_state(endpoint, cache_record=fresh_cache, verification_pending=True)

    assert request is not None
    assert request.reason == "reachable_candidate_verification_pending"


@pytest.mark.anyio
async def test_schedules_when_reachable_cache_is_missing_or_stale() -> None:
    sink = _Sink()
    service = _service(sink)
    endpoint = _endpoint(priority_reason=None)

    missing = await service.schedule_for_cache_if_needed(endpoint, cache_record=None)
    service = _service(sink)
    lifecycle = scheduler.DiscoveryStateMachine()
    lifecycle.markQueued(now_ms=1)
    lifecycle.markRunning(now_ms=2)
    lifecycle.markCompleted(now_ms=3)
    lifecycle.markStale(now_ms=4)
    stale = await service.schedule_for_cache_if_needed(
        endpoint,
        cache_record=_metadata(),
        previous_discovery_state=scheduler.PreviousDiscoveryState(lifecycle=lifecycle),
    )

    assert missing is not None
    assert missing.reason == "cache_missing"
    assert stale is not None
    assert stale.reason == "cache_stale"


@pytest.mark.anyio
async def test_planning_fingerprint_change_does_not_schedule_discovery() -> None:
    sink = _Sink()
    service = _service(sink)

    request = await service.schedule_for_cache_if_needed(
        _endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON),
        cache_record=_metadata(planning_fingerprint="old-plan"),
        planning_fingerprint="new-plan",
    )

    assert request is None
    assert sink.requests == []


@pytest.mark.anyio
async def test_schedules_user_refresh_and_verification_required_for_reachable_endpoint() -> None:
    sink = _Sink()
    service = _service(sink)
    endpoint = _endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON)

    refresh = await service.schedule_user_refresh(endpoint)
    service = _service(sink)
    verification = await service.schedule_verification_required(endpoint)

    assert refresh is not None
    assert refresh.reason == "user_refresh"
    assert verification is not None
    assert verification.reason == "verification_required"


@pytest.mark.anyio
@pytest.mark.parametrize(
    ("status", "priority_reason", "failure_code"),
    [
        ("offline", scheduler.REACHABLE_STABLE_REASON, None),
        ("reachable", scheduler.SUSPECT_OFFLINE_REASON, None),
        ("reachable", scheduler.REACHABLE_STABLE_REASON, "network_unreachable"),
        ("reachable", scheduler.REACHABLE_STABLE_REASON, "mms_unavailable"),
        ("not_applicable", None, None),
    ],
)
async def test_does_not_schedule_blocked_availability_states(status: str, priority_reason: str | None, failure_code: str | None) -> None:
    sink = _Sink()
    service = _service(sink)

    request = await service.on_watcher_state(
        _endpoint(status=status, priority_reason=priority_reason, failure_code=failure_code),
        verification_pending=True,
    )

    assert request is None
    assert sink.requests == []


@pytest.mark.anyio
async def test_repeated_same_reason_does_not_enqueue_duplicate_request() -> None:
    sink = _Sink()
    service = _service(sink)
    endpoint = _endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON)

    first = await service.on_watcher_state(endpoint)
    second = await service.on_watcher_state(endpoint)

    assert first is not None
    assert second is None
    assert len(sink.requests) == 1


@pytest.mark.anyio
async def test_existing_pending_discovery_suppresses_duplicate_reason_change() -> None:
    sink = _Sink()
    service = _service(sink)
    endpoint = _endpoint(priority_reason=None)

    first = await service.schedule_for_cache_if_needed(endpoint, cache_record=None)
    second = await service.on_watcher_state(_endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON))

    assert first is not None
    assert second is None
    assert len(sink.requests) == 1


@pytest.mark.anyio
async def test_user_refresh_bypasses_dedupe_for_queued_background_request() -> None:
    sink = _Sink()
    service = _service(sink)
    endpoint = _endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON)

    background = await service.on_watcher_state(endpoint)
    refresh = await service.schedule_user_refresh(endpoint)

    assert background is not None
    assert refresh is not None
    assert refresh.reason == "user_refresh"
    assert len(sink.requests) == 2


@pytest.mark.anyio
async def test_verification_required_upgrades_lower_priority_pending_request() -> None:
    sink = _Sink()
    service = _service(sink)
    endpoint = _endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON)

    background = await service.on_watcher_state(endpoint)
    verification = await service.schedule_verification_required(endpoint)

    assert background is not None
    assert verification is not None
    assert verification.reason == "verification_required"
    assert verification.priority == "high"
    assert len(sink.requests) == 2


@pytest.mark.anyio
async def test_redis_sink_allows_high_priority_upgrade_over_background_dedupe(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _Redis()
    monkeypatch.setattr(scheduler.RedisManager, "get_instance", lambda: redis)
    sink = scheduler.RedisExternalIedDiscoveryJobSink()
    background = scheduler.ExternalIedDiscoveryRequest(
        request_id="background",
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        ip="172.16.40.128",
        port=12447,
        signal_ids=(1,),
        reason="cache_missing",
        priority="normal",
        model_fingerprint=None,
        planning_fingerprint=None,
        requested_at_ms=100,
        earliest_execution_at_ms=100,
    )
    verification = scheduler.ExternalIedDiscoveryRequest(
        request_id="verification",
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        ip="172.16.40.128",
        port=12447,
        signal_ids=(1,),
        reason="verification_required",
        priority="high",
        model_fingerprint=None,
        planning_fingerprint=None,
        requested_at_ms=200,
        earliest_execution_at_ms=200,
    )

    await sink.enqueue(background)
    await sink.enqueue(verification)

    assert len(redis.streams["external-ied-discovery:jobs"]) == 2
    dedupe_payload = redis.values["external_ied:workspace:5:discovery_pending:172.16.40.128:12447"]
    assert '"request_id":"verification"' in dedupe_payload
    assert '"priority":"high"' in dedupe_payload
    assert '"reason":"verification_required"' in dedupe_payload
    assert '"requested_at_ms":200' in dedupe_payload
    assert '"planning_fingerprint":null' in dedupe_payload
    state_payload = redis.hashes["external_ied:workspace:5:discovery_state"]["172.16.40.128:12447"]
    assert '"queued_request_id":"verification"' in state_payload
    assert '"queued_priority":"high"' in state_payload


@pytest.mark.anyio
async def test_verification_dedupe_keeps_distinct_planning_contexts(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _Redis()
    monkeypatch.setattr(scheduler.RedisManager, "get_instance", lambda: redis)
    sink = scheduler.RedisExternalIedDiscoveryJobSink()
    first = scheduler.ExternalIedDiscoveryRequest(
        request_id="verification-a",
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        ip="172.16.40.128",
        port=12447,
        signal_ids=(1,),
        reason="verification_required",
        priority="high",
        model_fingerprint=None,
        planning_fingerprint="plan-a",
        requested_at_ms=100,
        earliest_execution_at_ms=100,
    )
    second = scheduler.ExternalIedDiscoveryRequest(
        request_id="verification-b",
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        ip="172.16.40.128",
        port=12447,
        signal_ids=(1,),
        reason="verification_required",
        priority="high",
        model_fingerprint=None,
        planning_fingerprint="plan-b",
        requested_at_ms=200,
        earliest_execution_at_ms=200,
    )

    await sink.enqueue(first)
    await sink.enqueue(second)

    assert len(redis.streams["external-ied-discovery:jobs"]) == 2
    assert '"planning_fingerprint":"plan-b"' in redis.values["external_ied:workspace:5:discovery_pending:172.16.40.128:12447"]


@pytest.mark.anyio
async def test_scheduler_does_not_enqueue_queued_or_running_lifecycle() -> None:
    sink = _Sink()
    service = _service(sink)
    queued = scheduler.DiscoveryStateMachine()
    queued.markQueued(now_ms=100)
    running = scheduler.DiscoveryStateMachine()
    running.markQueued(now_ms=100)
    running.markRunning(now_ms=110)

    first = await service.on_watcher_state(
        _endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON),
        previous_discovery_state=scheduler.PreviousDiscoveryState(lifecycle=queued),
    )
    second = await service.on_watcher_state(
        _endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON),
        previous_discovery_state=scheduler.PreviousDiscoveryState(lifecycle=running),
    )

    assert first is None
    assert second is None
    assert sink.requests == []


@pytest.mark.anyio
async def test_running_state_older_than_timeout_can_be_recovered() -> None:
    sink = _Sink()
    service = scheduler.ExternalIedDiscoveryScheduler(
        sink=sink,
        now_ms=lambda: 10_000,
        config=scheduler.ExternalIedDiscoverySchedulerConfig(running_timeout_ms=1000),
    )
    running = scheduler.DiscoveryStateMachine()
    running.markQueued(now_ms=1_000, request_id="old", priority="normal", reason="cache_missing")
    running.markRunning(now_ms=2_000)

    request = await service.on_watcher_state(
        _endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON),
        previous_discovery_state=scheduler.PreviousDiscoveryState(lifecycle=running),
    )

    assert request is None
    recovered = service.discovery_states.get("5:172.16.40.128:12447")
    assert recovered is None or recovered.state != "Running"


@pytest.mark.anyio
async def test_retry_waiting_blocks_scheduling_until_retry_timer() -> None:
    sink = _Sink()
    service = _service(sink)
    retry = scheduler.DiscoveryStateMachine()
    retry.markQueued(now_ms=100)
    retry.markRunning(now_ms=110)
    retry.markRetry(now_ms=120, retry_at_ms=200_000, error="timeout")

    blocked = await service.on_watcher_state(
        _endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON),
        previous_discovery_state=scheduler.PreviousDiscoveryState(lifecycle=retry),
    )

    assert blocked is None
    assert sink.requests == []


@pytest.mark.anyio
async def test_retry_waiting_after_timer_retries_original_purpose_even_with_valid_cache() -> None:
    sink = _Sink()
    service = _service(sink)
    retry = scheduler.DiscoveryStateMachine()
    retry.markQueued(now_ms=100, request_id="old", priority="high", reason="user_refresh")
    retry.markRunning(now_ms=110)
    retry.markRetry(now_ms=120, retry_at_ms=123_000, error="timeout")

    request = await service.on_watcher_state(
        _endpoint(priority_reason=scheduler.REACHABLE_STABLE_REASON),
        cache_record=_metadata(),
        previous_discovery_state=scheduler.PreviousDiscoveryState(lifecycle=retry),
    )

    assert request is not None
    assert request.reason == "user_refresh"
    assert request.priority == "high"


def test_discovery_ui_fields_from_payload_exposes_lifecycle_state() -> None:
    fields = scheduler.discovery_ui_fields_from_payload({
        "state": "Succeeded",
        "updated_at_ms": 123,
    })

    assert fields["discovery_state"] == "Succeeded"
    assert fields["discovery_ready_for_verification"] is True
    assert fields["discovery_updated_at_ms"] == 123


def test_discovery_cache_metadata_parses_required_fields_without_project_config_fingerprint() -> None:
    metadata = scheduler.discovery_cache_metadata_from_payload({
        "discovery_version": "unitlab-discovery.v1",
        "device_identity": "IED-A",
        "vendor": "UnitLab",
        "model": "VirtualIED",
        "configRev": "7",
        "last_discovery_at_ms": 100,
        "last_successful_discovery_at_ms": 100,
        "last_failed_discovery_at_ms": 90,
        "last_error": "previous failure",
        "discovery_duration_ms": 32,
        "model_fingerprint": "actual-model-fingerprint",
        "planning_fingerprint": "planner-fingerprint",
        "ip": "10.10.10.20",
        "port": 102,
        "signal_ids": [1, 2, 3],
    })

    assert metadata is not None
    assert metadata.config_rev == "7"
    assert metadata.model_fingerprint == "actual-model-fingerprint"
    assert metadata.planning_fingerprint == "planner-fingerprint"
    assert "ip" not in metadata.to_payload()
    assert "port" not in metadata.to_payload()
    assert "signal_ids" not in metadata.to_payload()


def test_scheduler_does_not_own_tcp_probe_implementation() -> None:
    assert not hasattr(scheduler, "check_mms_tcp_endpoint")
    assert not hasattr(scheduler, "check_mms_endpoint")
    assert not hasattr(scheduler, "_probe_state")
    assert not hasattr(scheduler, "discovery_fingerprint")
