from __future__ import annotations

import asyncio
import heapq
import json
import random
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any

from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.schemas.verification_schema import VerificationMmsReachabilityTargetSchema
from app.schemas.ws.events import (
    ExternalIedStatusChangedEvent,
    ExternalIedStatusRecord,
    ExternalIedStatusSnapshotEvent,
)
from app.services.verification_mms_reachability import check_mms_tcp_endpoint

logger = get_logger("external_ied")

EXPECTED_POLL_MS = 1_200
OFFLINE_POLL_MS = 5_000
REACHABLE_POLL_MS = 30_000
CHECK_TIMEOUT_MS = 1_200
MAX_CONCURRENT_CHECKS = 3
TARGET_WORKSPACES_KEY = "external_ied:workspaces"

PRIORITY_VERIFICATION_NEEDED = 0
PRIORITY_RECENT_STATE_CHANGE = 1
PRIORITY_EXPECTED = 2
PRIORITY_REACHABLE_CANDIDATE = 3
PRIORITY_SUSPECT_OFFLINE = 4
PRIORITY_OFFLINE = 5
PRIORITY_REACHABLE_STABLE = 6

PRIORITY_REASON_VERIFICATION_NEEDED = "verification_needed"
PRIORITY_REASON_RECENT_STATE_CHANGE = "recent_state_change"
PRIORITY_REASON_EXPECTED = "expected_first_pass"
PRIORITY_REASON_REACHABLE_CANDIDATE = "reachable_candidate"
PRIORITY_REASON_SUSPECT_OFFLINE = "suspect_offline"
PRIORITY_REASON_OFFLINE = "offline"
PRIORITY_REASON_REACHABLE_STABLE = "reachable_stable"


@dataclass(frozen=True)
class ExternalIedTarget:
    ip: str
    port: int
    signal_ids: tuple[int, ...]


@dataclass(frozen=True)
class ExternalIedWatcherConfig:
    verification_needed_interval_ms: int = 1_000
    verification_needed_timeout_ms: int = 10_000
    recent_state_change_interval_ms: int = 1_500
    expected_first_pass_interval_ms: int = 0
    reachable_candidate_interval_ms: int = 2_000
    suspect_offline_interval_ms: int = 1_500
    offline_interval_ms: int = OFFLINE_POLL_MS
    reachable_stable_interval_ms: int = REACHABLE_POLL_MS
    interval_jitter_ratio: float = 0.15
    ageing_step_ms: int = 10_000
    max_ageing_priority_boost: int = 3
    cold_start_duration_ms: int = 5_000
    cold_start_probe_interval_ms: int = 0
    timeout_ms: int = CHECK_TIMEOUT_MS
    max_concurrent_checks: int = MAX_CONCURRENT_CHECKS
    stable_success_threshold: int = 2
    offline_failure_threshold: int = 2
    target_sync_interval_ms: int = 1_000
    loop_idle_sleep_ms: int = 250


@dataclass
class EndpointProbeState:
    endpoint_id: str
    workspace_id: int
    host: str
    port: int
    signal_ids: tuple[int, ...]
    status: str
    last_probe_at_ms: int | None
    next_probe_at_ms: int
    last_success_at_ms: int | None
    last_failure_at_ms: int | None
    consecutive_successes: int
    consecutive_failures: int
    active_probe: bool
    priority_reason: str
    queue_version: int = 0


@dataclass(frozen=True)
class ProbeTransition:
    state: EndpointProbeState
    record: ExternalIedStatusRecord
    old_status: str
    new_status: str
    status_changed: bool
    next_check_at_ms: int
    probe_succeeded: bool


@dataclass(frozen=True, order=True)
class _QueueEntry:
    next_probe_at_ms: int
    priority: int
    last_probe_sort_ms: int
    version: int
    endpoint_id: str


def _targets_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:targets"


def _status_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:status"


def _lock_key(workspace_id: int, ip: str) -> str:
    return f"external_ied:workspace:{workspace_id}:lock:{ip}"


def _endpoint_key(ip: str, port: int) -> str:
    return f"{ip}:{port}"


def _scheduler_endpoint_id(workspace_id: int, endpoint: str) -> str:
    return f"{workspace_id}:{endpoint}"


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def _normalize_ip(value: Any) -> str | None:
    text = str(value or "").strip()
    parts = text.split(".")
    if len(parts) != 4:
        return None
    try:
        octets = [int(part, 10) for part in parts]
    except ValueError:
        return None
    if any(octet < 0 or octet > 255 for octet in octets):
        return None
    return ".".join(str(octet) for octet in octets)


def _normalize_signal_ids(values: Any) -> tuple[int, ...]:
    if not isinstance(values, list | tuple):
        return ()
    seen: set[int] = set()
    normalized: list[int] = []
    for raw in values:
        try:
            signal_id = int(raw)
        except (TypeError, ValueError):
            continue
        if signal_id <= 0 or signal_id in seen:
            continue
        seen.add(signal_id)
        normalized.append(signal_id)
    return tuple(sorted(normalized))


def _normalize_port(value: Any) -> int:
    try:
        port = int(value)
    except (TypeError, ValueError):
        return 102
    return port if 1 <= port <= 65535 else 102


def _signal_ids_from_json_payload(payload: str | None) -> tuple[int, ...]:
    if not payload:
        return ()
    try:
        parsed = json.loads(payload)
    except json.JSONDecodeError:
        return ()
    if not isinstance(parsed, dict):
        return ()
    return _normalize_signal_ids(parsed.get("signal_ids"))


def _target_from_json_payload(endpoint: str, payload: str | None) -> ExternalIedTarget | None:
    if not payload:
        return None
    try:
        parsed = json.loads(payload)
    except json.JSONDecodeError:
        return None
    if not isinstance(parsed, dict):
        return None
    ip = _normalize_ip(parsed.get("ip"))
    if not ip:
        ip = _normalize_ip(endpoint.rsplit(":", 1)[0])
    if not ip:
        return None
    signal_ids = _normalize_signal_ids(parsed.get("signal_ids"))
    if not signal_ids:
        return None
    return ExternalIedTarget(ip=ip, port=_normalize_port(parsed.get("port")), signal_ids=signal_ids)


def normalize_external_ied_targets(raw_targets: list[Any]) -> dict[str, ExternalIedTarget]:
    targets: dict[str, ExternalIedTarget] = {}
    for raw in raw_targets:
        if not isinstance(raw, dict):
            continue
        ip = _normalize_ip(raw.get("ip"))
        port = _normalize_port(raw.get("port"))
        signal_ids = _normalize_signal_ids(raw.get("signal_ids"))
        if not ip or not signal_ids:
            continue
        key = _endpoint_key(ip, port)
        existing = targets.get(key)
        merged = tuple(sorted(set(signal_ids).union(existing.signal_ids if existing else ())))
        targets[key] = ExternalIedTarget(ip=ip, port=port, signal_ids=merged)
    return targets


def _record_from_payload(ip: str, payload: str | None, target: ExternalIedTarget | None = None) -> ExternalIedStatusRecord:
    parsed: dict[str, Any] = {}
    if payload:
        try:
            candidate = json.loads(payload)
            if isinstance(candidate, dict):
                parsed = candidate
        except json.JSONDecodeError:
            parsed = {}
    signal_ids = list(target.signal_ids if target else _normalize_signal_ids(parsed.get("signal_ids")))
    return ExternalIedStatusRecord(
        ip=ip,
        port=target.port if target else _normalize_port(parsed.get("port")),
        status=parsed.get("status") if parsed.get("status") in {"unknown", "expected", "reachable", "offline"} else "expected",
        signal_ids=signal_ids,
        last_checked_at=parsed.get("last_checked_at") if isinstance(parsed.get("last_checked_at"), str) else None,
        last_error=parsed.get("last_error") if isinstance(parsed.get("last_error"), str) else None,
        check_kind=parsed.get("check_kind") if parsed.get("check_kind") in {"none", "tcp_connect"} else "none",
        failure_code=parsed.get("failure_code") if parsed.get("failure_code") in {"unreachable", "mms_unavailable", "network_unreachable", "probe_failed"} else None,
    )


def _serialize_record(record: ExternalIedStatusRecord, *, next_check_at_ms: int | None = None) -> str:
    payload = record.model_dump(mode="json")
    if next_check_at_ms is not None:
        payload["next_check_at_ms"] = next_check_at_ms
    return json.dumps(payload, separators=(",", ":"))


def _serialize_state_record(state: EndpointProbeState, record: ExternalIedStatusRecord) -> str:
    payload = record.model_dump(mode="json")
    payload.update({
        "next_check_at_ms": state.next_probe_at_ms,
        "last_probe_at_ms": state.last_probe_at_ms,
        "last_success_at_ms": state.last_success_at_ms,
        "last_failure_at_ms": state.last_failure_at_ms,
        "consecutive_successes": state.consecutive_successes,
        "consecutive_failures": state.consecutive_failures,
        "priority_reason": state.priority_reason,
    })
    return json.dumps(payload, separators=(",", ":"))


def _int_or_none(value: Any) -> int | None:
    try:
        parsed = int(value)
    except (TypeError, ValueError):
        return None
    return parsed if parsed >= 0 else None


def _priority_for_state(state: EndpointProbeState) -> int:
    if state.priority_reason == PRIORITY_REASON_VERIFICATION_NEEDED:
        return PRIORITY_VERIFICATION_NEEDED
    if state.priority_reason == PRIORITY_REASON_RECENT_STATE_CHANGE:
        return PRIORITY_RECENT_STATE_CHANGE
    if state.priority_reason == PRIORITY_REASON_EXPECTED:
        return PRIORITY_EXPECTED
    if state.priority_reason == PRIORITY_REASON_REACHABLE_CANDIDATE:
        return PRIORITY_REACHABLE_CANDIDATE
    if state.priority_reason == PRIORITY_REASON_SUSPECT_OFFLINE:
        return PRIORITY_SUSPECT_OFFLINE
    if state.priority_reason == PRIORITY_REASON_OFFLINE:
        return PRIORITY_OFFLINE
    return PRIORITY_REACHABLE_STABLE


def _interval_for_priority_reason(reason: str, config: ExternalIedWatcherConfig) -> int:
    if reason == PRIORITY_REASON_VERIFICATION_NEEDED:
        return config.verification_needed_interval_ms
    if reason == PRIORITY_REASON_RECENT_STATE_CHANGE:
        return config.recent_state_change_interval_ms
    if reason == PRIORITY_REASON_EXPECTED:
        return config.expected_first_pass_interval_ms
    if reason == PRIORITY_REASON_REACHABLE_CANDIDATE:
        return config.reachable_candidate_interval_ms
    if reason == PRIORITY_REASON_SUSPECT_OFFLINE:
        return config.suspect_offline_interval_ms
    if reason == PRIORITY_REASON_OFFLINE:
        return config.offline_interval_ms
    return config.reachable_stable_interval_ms


def _apply_interval_jitter(interval_ms: int, *, config: ExternalIedWatcherConfig) -> int:
    if interval_ms <= 0:
        return 0
    ratio = max(0.0, min(float(config.interval_jitter_ratio), 0.5))
    if ratio <= 0:
        return interval_ms
    return max(0, int(round(interval_ms * random.uniform(1.0 - ratio, 1.0 + ratio))))


def _next_probe_at_for_reason(
    reason: str,
    *,
    now_ms: int,
    config: ExternalIedWatcherConfig,
) -> int:
    return now_ms + _apply_interval_jitter(_interval_for_priority_reason(reason, config), config=config)


def _effective_priority_for_state(
    state: EndpointProbeState,
    *,
    now_ms: int,
    config: ExternalIedWatcherConfig,
) -> int:
    base_priority = _priority_for_state(state)
    if state.next_probe_at_ms > now_ms:
        return base_priority
    step_ms = max(1, int(config.ageing_step_ms))
    max_boost = max(0, int(config.max_ageing_priority_boost))
    overdue_boost = min(max_boost, max(0, now_ms - state.next_probe_at_ms) // step_ms)
    return max(PRIORITY_VERIFICATION_NEEDED, base_priority - overdue_boost)


def _priority_reason_for_record(status: str, consecutive_successes: int, consecutive_failures: int, config: ExternalIedWatcherConfig) -> str:
    if status in {"expected", "unknown"}:
        return PRIORITY_REASON_EXPECTED
    if status == "offline":
        return PRIORITY_REASON_OFFLINE
    if consecutive_failures > 0:
        return PRIORITY_REASON_SUSPECT_OFFLINE
    if consecutive_successes >= config.stable_success_threshold:
        return PRIORITY_REASON_REACHABLE_STABLE
    return PRIORITY_REASON_REACHABLE_CANDIDATE


def _state_from_payload(
    *,
    workspace_id: int,
    endpoint: str,
    target: ExternalIedTarget,
    payload: str | None,
    now_ms: int,
    config: ExternalIedWatcherConfig,
) -> EndpointProbeState:
    parsed: dict[str, Any] = {}
    if payload:
        try:
            candidate = json.loads(payload)
            if isinstance(candidate, dict):
                parsed = candidate
        except json.JSONDecodeError:
            parsed = {}
    record = _record_from_payload(target.ip, payload, target)
    consecutive_successes = _int_or_none(parsed.get("consecutive_successes")) or (1 if record.status == "reachable" and record.last_checked_at else 0)
    consecutive_failures = _int_or_none(parsed.get("consecutive_failures")) or (1 if record.status == "offline" and record.last_checked_at else 0)
    priority_reason = parsed.get("priority_reason")
    if priority_reason not in {
        PRIORITY_REASON_VERIFICATION_NEEDED,
        PRIORITY_REASON_RECENT_STATE_CHANGE,
        PRIORITY_REASON_EXPECTED,
        PRIORITY_REASON_REACHABLE_CANDIDATE,
        PRIORITY_REASON_SUSPECT_OFFLINE,
        PRIORITY_REASON_OFFLINE,
        PRIORITY_REASON_REACHABLE_STABLE,
    }:
        priority_reason = _priority_reason_for_record(record.status, consecutive_successes, consecutive_failures, config)
    return EndpointProbeState(
        endpoint_id=_scheduler_endpoint_id(workspace_id, endpoint),
        workspace_id=workspace_id,
        host=target.ip,
        port=target.port,
        signal_ids=target.signal_ids,
        status=record.status,
        last_probe_at_ms=_int_or_none(parsed.get("last_probe_at_ms")),
        next_probe_at_ms=_int_or_none(parsed.get("next_check_at_ms")) or now_ms,
        last_success_at_ms=_int_or_none(parsed.get("last_success_at_ms")),
        last_failure_at_ms=_int_or_none(parsed.get("last_failure_at_ms")),
        consecutive_successes=consecutive_successes,
        consecutive_failures=consecutive_failures,
        active_probe=False,
        priority_reason=str(priority_reason),
    )


class ExternalIedPriorityScheduler:
    def __init__(self, config: ExternalIedWatcherConfig | None = None) -> None:
        self.config = config or ExternalIedWatcherConfig()
        self._states: dict[str, EndpointProbeState] = {}
        self._queue: list[_QueueEntry] = []
        self._verification_needed: dict[str, int] = {}
        self._started_at_ms: int | None = None

    @property
    def states(self) -> dict[str, EndpointProbeState]:
        return self._states

    @property
    def queue_size(self) -> int:
        return len(self._queue)

    def set_targets(self, states: list[EndpointProbeState], *, now_ms: int) -> None:
        if self._started_at_ms is None:
            self._started_at_ms = now_ms
        self._prune_verification_needed(now_ms)
        next_ids = {state.endpoint_id for state in states}
        for endpoint_id in list(self._states):
            if endpoint_id not in next_ids:
                self._states.pop(endpoint_id, None)
                self._verification_needed.pop(endpoint_id, None)

        for state in states:
            existing = self._states.get(state.endpoint_id)
            if existing is not None:
                state.last_probe_at_ms = existing.last_probe_at_ms
                state.last_success_at_ms = existing.last_success_at_ms
                state.last_failure_at_ms = existing.last_failure_at_ms
                state.consecutive_successes = existing.consecutive_successes
                state.consecutive_failures = existing.consecutive_failures
                state.active_probe = existing.active_probe
                state.priority_reason = existing.priority_reason
                state.next_probe_at_ms = min(existing.next_probe_at_ms, state.next_probe_at_ms)
            elif self._in_cold_start(now_ms):
                state.next_probe_at_ms = min(state.next_probe_at_ms, now_ms + self.config.cold_start_probe_interval_ms)
            if state.endpoint_id in self._verification_needed:
                state.priority_reason = PRIORITY_REASON_VERIFICATION_NEEDED
                state.next_probe_at_ms = now_ms
            self._upsert(state)

    def markVerificationNeeded(self, endpoint_ids: set[str] | list[str] | tuple[str, ...], *, now_ms: int | None = None) -> None:
        now = now_ms if now_ms is not None else int(time.time() * 1000)
        for endpoint_id in endpoint_ids:
            self._verification_needed[endpoint_id] = now + self.config.verification_needed_timeout_ms
            state = self._states.get(endpoint_id)
            if state is None:
                continue
            state.priority_reason = PRIORITY_REASON_VERIFICATION_NEEDED
            state.next_probe_at_ms = now
            self._upsert(state)

    def clearVerificationNeeded(self, endpoint_ids: set[str] | list[str] | tuple[str, ...], *, now_ms: int | None = None) -> None:
        now = now_ms if now_ms is not None else int(time.time() * 1000)
        for endpoint_id in endpoint_ids:
            self._verification_needed.pop(endpoint_id, None)
            state = self._states.get(endpoint_id)
            if state is None:
                continue
            state.priority_reason = _priority_reason_for_record(
                state.status,
                state.consecutive_successes,
                state.consecutive_failures,
                self.config,
            )
            state.next_probe_at_ms = _next_probe_at_for_reason(state.priority_reason, now_ms=now, config=self.config)
            self._upsert(state)

    def boostEndpoint(self, endpoint_id: str, reason: str = PRIORITY_REASON_RECENT_STATE_CHANGE, *, now_ms: int | None = None) -> None:
        state = self._states.get(endpoint_id)
        if state is None:
            return
        now = now_ms if now_ms is not None else int(time.time() * 1000)
        state.priority_reason = reason
        state.next_probe_at_ms = now
        self._upsert(state)

    def onEndpointStateChanged(self, endpoint_id: str, old_status: str, new_status: str, *, now_ms: int | None = None) -> None:
        if old_status == new_status:
            return
        self.boostEndpoint(endpoint_id, PRIORITY_REASON_RECENT_STATE_CHANGE, now_ms=now_ms)

    def pop_due(self, *, now_ms: int, max_count: int) -> list[EndpointProbeState]:
        self._prune_verification_needed(now_ms)
        due_entries: list[tuple[_QueueEntry, EndpointProbeState]] = []
        while self._queue:
            entry = heapq.heappop(self._queue)
            state = self._states.get(entry.endpoint_id)
            if state is None:
                continue
            if state.queue_version != entry.version:
                continue
            if state.active_probe:
                continue
            if state.next_probe_at_ms > now_ms:
                heapq.heappush(self._queue, entry)
                break
            due_entries.append((entry, state))

        if not due_entries:
            return []

        due_entries.sort(key=lambda item: (
            _effective_priority_for_state(item[1], now_ms=now_ms, config=self.config),
            item[1].next_probe_at_ms,
            item[1].last_probe_at_ms if item[1].last_probe_at_ms is not None else -1,
            item[1].endpoint_id,
        ))
        selected = due_entries[:max_count]
        deferred = due_entries[max_count:]

        for entry, _state in deferred:
            heapq.heappush(self._queue, entry)

        due: list[EndpointProbeState] = []
        for _entry, state in selected:
            state.active_probe = True
            state.queue_version += 1
            due.append(state)
        return due

    def complete_probe(self, endpoint_id: str, transition: ProbeTransition) -> None:
        state = transition.state
        state.active_probe = False
        verification_expires_at = self._verification_needed.get(state.endpoint_id)
        if verification_expires_at is not None and transition.probe_succeeded:
            self._verification_needed.pop(state.endpoint_id, None)
        elif verification_expires_at is not None and verification_expires_at > (state.last_probe_at_ms or 0):
            state.priority_reason = PRIORITY_REASON_VERIFICATION_NEEDED
            state.next_probe_at_ms = _next_probe_at_for_reason(
                PRIORITY_REASON_VERIFICATION_NEEDED,
                now_ms=state.last_probe_at_ms or transition.next_check_at_ms,
                config=self.config,
            )
        elif verification_expires_at is not None:
            self._verification_needed.pop(state.endpoint_id, None)
        self._upsert(state)

    def fail_probe(self, endpoint_id: str, *, now_ms: int) -> None:
        state = self._states.get(endpoint_id)
        if state is None:
            return
        state.active_probe = False
        state.next_probe_at_ms = _next_probe_at_for_reason(PRIORITY_REASON_SUSPECT_OFFLINE, now_ms=now_ms, config=self.config)
        self._upsert(state)

    def next_due_delay_ms(self, *, now_ms: int) -> int:
        while self._queue:
            entry = self._queue[0]
            state = self._states.get(entry.endpoint_id)
            if state is None or state.queue_version != entry.version:
                heapq.heappop(self._queue)
                continue
            return max(0, state.next_probe_at_ms - now_ms)
        return self.config.loop_idle_sleep_ms

    def _in_cold_start(self, now_ms: int) -> bool:
        return (
            self._started_at_ms is not None
            and self.config.cold_start_duration_ms > 0
            and now_ms - self._started_at_ms <= self.config.cold_start_duration_ms
        )

    def _prune_verification_needed(self, now_ms: int) -> None:
        for endpoint_id, expires_at in list(self._verification_needed.items()):
            if expires_at > now_ms:
                continue
            self._verification_needed.pop(endpoint_id, None)
            state = self._states.get(endpoint_id)
            if state is None or state.priority_reason != PRIORITY_REASON_VERIFICATION_NEEDED:
                continue
            state.priority_reason = _priority_reason_for_record(
                state.status,
                state.consecutive_successes,
                state.consecutive_failures,
                self.config,
            )
            state.next_probe_at_ms = _next_probe_at_for_reason(state.priority_reason, now_ms=now_ms, config=self.config)
            self._upsert(state)

    def _upsert(self, state: EndpointProbeState) -> None:
        state.queue_version += 1
        self._states[state.endpoint_id] = state
        heapq.heappush(
            self._queue,
            _QueueEntry(
                next_probe_at_ms=state.next_probe_at_ms,
                priority=_priority_for_state(state),
                last_probe_sort_ms=state.last_probe_at_ms if state.last_probe_at_ms is not None else -1,
                version=state.queue_version,
                endpoint_id=state.endpoint_id,
            ),
        )


def resolve_probe_transition(
    state: EndpointProbeState,
    result: Any,
    *,
    now_ms: int,
    config: ExternalIedWatcherConfig,
) -> ProbeTransition:
    old_status = state.status if state.status in {"unknown", "expected", "reachable", "offline"} else "expected"
    if result.reachable:
        state.consecutive_successes += 1
        state.consecutive_failures = 0
        state.last_success_at_ms = now_ms
        new_status = "reachable"
        state.priority_reason = (
            PRIORITY_REASON_REACHABLE_STABLE
            if state.consecutive_successes >= config.stable_success_threshold
            else PRIORITY_REASON_REACHABLE_CANDIDATE
        )
    else:
        state.consecutive_failures += 1
        state.consecutive_successes = 0
        state.last_failure_at_ms = now_ms
        if old_status == "reachable" and state.consecutive_failures < config.offline_failure_threshold:
            new_status = "reachable"
            state.priority_reason = PRIORITY_REASON_SUSPECT_OFFLINE
        else:
            new_status = "offline"
            state.priority_reason = PRIORITY_REASON_OFFLINE

    state.status = new_status
    state.last_probe_at_ms = now_ms
    state.next_probe_at_ms = _next_probe_at_for_reason(state.priority_reason, now_ms=now_ms, config=config)
    record = ExternalIedStatusRecord(
        ip=state.host,
        port=state.port,
        status=new_status,
        signal_ids=list(state.signal_ids),
        last_checked_at=result.checked_at,
        last_error=result.error,
        check_kind=result.check_kind,
        failure_code=result.failure_code,
    )
    return ProbeTransition(
        state=state,
        record=record,
        old_status=old_status,
        new_status=new_status,
        status_changed=old_status != new_status,
        next_check_at_ms=state.next_probe_at_ms,
        probe_succeeded=result.reachable,
    )


async def configure_external_ied_targets(workspace_id: int, raw_targets: list[Any]) -> ExternalIedStatusSnapshotEvent:
    redis = RedisManager.get_instance()
    targets = normalize_external_ied_targets(raw_targets)
    target_key = _targets_key(workspace_id)
    status_key = _status_key(workspace_id)
    previous_targets = await redis.hgetall(target_key)
    previous_statuses = await redis.hgetall(status_key)
    previous_signal_ids = [
        signal_id
        for payload in previous_targets.values()
        for signal_id in _signal_ids_from_json_payload(payload)
    ]

    if not targets:
        if previous_targets or previous_statuses:
            await redis.delete(target_key, status_key)
            await redis.srem(TARGET_WORKSPACES_KEY, str(workspace_id))
            logger.info("External IED targets cleared | workspace=%s", workspace_id)
        event = ExternalIedStatusSnapshotEvent(
            workspace_id=workspace_id,
            devices=[],
            removed_signal_ids=sorted(set(previous_signal_ids)),
            emitted_at=_utc_now_iso(),
        )
        if previous_targets or previous_statuses:
            await WsEventPublisher.publish(event)
        return event

    await redis.sadd(TARGET_WORKSPACES_KEY, str(workspace_id))
    pipe = redis.pipeline()
    pipe.delete(target_key)
    pipe.delete(status_key)
    devices: list[ExternalIedStatusRecord] = []
    now_ms = int(time.time() * 1000)
    for key, target in targets.items():
        record = _record_from_payload(target.ip, previous_statuses.get(key), target)
        devices.append(record)
        pipe.hset(target_key, key, json.dumps({"ip": target.ip, "port": target.port, "signal_ids": list(target.signal_ids)}, separators=(",", ":")))
        pipe.hset(status_key, key, _serialize_record(record, next_check_at_ms=now_ms))
    await pipe.execute()

    endpoint_sample = ", ".join(sorted(targets.keys())[:8])
    logger.info(
        "External IED targets configured | workspace=%s endpoints=%s sample=%s",
        workspace_id,
        len(targets),
        endpoint_sample,
    )

    event = ExternalIedStatusSnapshotEvent(
        workspace_id=workspace_id,
        devices=sorted(devices, key=lambda item: tuple(int(part) for part in item.ip.split("."))),
        removed_signal_ids=sorted(set(previous_signal_ids).difference(signal_id for target in targets.values() for signal_id in target.signal_ids)),
        emitted_at=_utc_now_iso(),
    )
    await WsEventPublisher.publish(event)
    return event


async def list_external_ied_status_snapshots() -> list[ExternalIedStatusSnapshotEvent]:
    redis = RedisManager.get_instance()
    raw_workspace_ids = await redis.smembers(TARGET_WORKSPACES_KEY)
    events: list[ExternalIedStatusSnapshotEvent] = []
    for raw_workspace_id in raw_workspace_ids:
        try:
            workspace_id = int(raw_workspace_id)
        except (TypeError, ValueError):
            continue
        statuses = await redis.hgetall(_status_key(workspace_id))
        devices = [_record_from_payload(endpoint.rsplit(":", 1)[0], payload) for endpoint, payload in statuses.items()]
        events.append(
            ExternalIedStatusSnapshotEvent(
                workspace_id=workspace_id,
                devices=sorted(devices, key=lambda item: tuple(int(part) for part in item.ip.split("."))),
                removed_signal_ids=[],
                emitted_at=_utc_now_iso(),
            )
        )
    return events


async def _probe_state(
    state: EndpointProbeState,
    *,
    config: ExternalIedWatcherConfig,
) -> ProbeTransition | None:
    redis = RedisManager.get_instance()
    endpoint = _endpoint_key(state.host, state.port)
    locked = await redis.set(_lock_key(state.workspace_id, endpoint), "1", nx=True, ex=10)
    if not locked:
        return None

    try:
        logger.debug(
            "External IED TCP probe started | workspace=%s endpoint=%s timeout_ms=%s",
            state.workspace_id,
            endpoint,
            config.timeout_ms,
        )
        result = await check_mms_tcp_endpoint(
            VerificationMmsReachabilityTargetSchema(host=state.host, port=state.port),
            timeout_ms=config.timeout_ms,
        )
        logger.debug(
            "External IED TCP probe completed | workspace=%s endpoint=%s reachable=%s failure=%s error=%s checked_at=%s",
            state.workspace_id,
            endpoint,
            result.reachable,
            result.failure_code,
            result.error,
            result.checked_at,
        )
        transition = resolve_probe_transition(
            state,
            result,
            now_ms=int(time.time() * 1000),
            config=config,
        )
        await redis.hset(
            _status_key(state.workspace_id),
            endpoint,
            _serialize_state_record(transition.state, transition.record),
        )
        if not transition.status_changed:
            return transition
        logger.info(
            "External IED status changed | workspace=%s endpoint=%s old=%s new=%s failure=%s",
            state.workspace_id,
            endpoint,
            transition.old_status,
            transition.new_status,
            result.failure_code,
        )
        await WsEventPublisher.publish(
            ExternalIedStatusChangedEvent(
                workspace_id=state.workspace_id,
                ip=state.host,
                port=state.port,
                old_status=transition.old_status,
                new_status=transition.new_status,
                signal_ids=list(state.signal_ids),
                checked_at=result.checked_at,
                check_kind=result.check_kind,
                failure_code=result.failure_code,
                error=result.error,
            )
        )
        return transition
    finally:
        await redis.delete(_lock_key(state.workspace_id, endpoint))


async def _check_one(workspace_id: int, endpoint: str, target: ExternalIedTarget, previous: ExternalIedStatusRecord) -> None:
    config = ExternalIedWatcherConfig()
    now_ms = int(time.time() * 1000)
    state = EndpointProbeState(
        endpoint_id=_scheduler_endpoint_id(workspace_id, endpoint),
        workspace_id=workspace_id,
        host=target.ip,
        port=target.port,
        signal_ids=target.signal_ids,
        status=previous.status,
        last_probe_at_ms=None,
        next_probe_at_ms=now_ms,
        last_success_at_ms=None,
        last_failure_at_ms=None,
        consecutive_successes=1 if previous.status == "reachable" and previous.last_checked_at else 0,
        consecutive_failures=1 if previous.status == "offline" and previous.last_checked_at else 0,
        active_probe=False,
        priority_reason=_priority_reason_for_record(previous.status, 0, 0, config),
    )
    await _probe_state(state, config=config)


async def _load_scheduler_states(config: ExternalIedWatcherConfig) -> list[EndpointProbeState]:
    redis = RedisManager.get_instance()
    raw_workspace_ids = await redis.smembers(TARGET_WORKSPACES_KEY)
    now_ms = int(time.time() * 1000)
    states: list[EndpointProbeState] = []
    for raw_workspace_id in raw_workspace_ids:
        try:
            workspace_id = int(raw_workspace_id)
        except (TypeError, ValueError):
            continue
        target_payloads = await redis.hgetall(_targets_key(workspace_id))
        status_payloads = await redis.hgetall(_status_key(workspace_id))
        for endpoint, payload in target_payloads.items():
            target = _target_from_json_payload(endpoint, payload)
            if target is None or not target.signal_ids:
                continue
            states.append(_state_from_payload(
                workspace_id=workspace_id,
                endpoint=endpoint,
                target=target,
                payload=status_payloads.get(endpoint),
                now_ms=now_ms,
                config=config,
            ))
    return states


async def run_external_ied_availability_checker(
    stop_event: asyncio.Event | None = None,
    *,
    config: ExternalIedWatcherConfig | None = None,
) -> None:
    watcher_config = config or ExternalIedWatcherConfig()
    scheduler = ExternalIedPriorityScheduler(watcher_config)
    active_tasks: dict[asyncio.Task[tuple[str, ProbeTransition | None]], str] = {}
    next_sync_at_ms = 0

    async def guarded_probe(state: EndpointProbeState) -> tuple[str, ProbeTransition | None]:
        try:
            return state.endpoint_id, await _probe_state(state, config=watcher_config)
        except Exception as exc:  # noqa: BLE001
            logger.warning("External IED TCP check failed | workspace=%s endpoint=%s:%s error=%s", state.workspace_id, state.host, state.port, exc)
            return state.endpoint_id, None

    while stop_event is None or not stop_event.is_set():
        now_ms = int(time.time() * 1000)
        if now_ms >= next_sync_at_ms:
            scheduler.set_targets(await _load_scheduler_states(watcher_config), now_ms=now_ms)
            next_sync_at_ms = now_ms + watcher_config.target_sync_interval_ms

        for task in [task for task in active_tasks if task.done()]:
            endpoint_id = active_tasks.pop(task)
            try:
                _, transition = task.result()
            except Exception as exc:  # noqa: BLE001
                scheduler.fail_probe(endpoint_id, now_ms=int(time.time() * 1000))
                logger.warning("External IED TCP task failed | endpoint_id=%s error=%s", endpoint_id, exc)
                continue
            if transition is None:
                scheduler.fail_probe(endpoint_id, now_ms=int(time.time() * 1000))
            else:
                scheduler.complete_probe(endpoint_id, transition)

        available_slots = max(0, watcher_config.max_concurrent_checks - len(active_tasks))
        if available_slots > 0:
            for state in scheduler.pop_due(now_ms=int(time.time() * 1000), max_count=available_slots):
                task = asyncio.create_task(guarded_probe(state))
                active_tasks[task] = state.endpoint_id

        wait_ms = min(
            watcher_config.loop_idle_sleep_ms,
            scheduler.next_due_delay_ms(now_ms=int(time.time() * 1000)),
            max(0, next_sync_at_ms - int(time.time() * 1000)),
        )
        if active_tasks:
            done, _pending = await asyncio.wait(
                active_tasks.keys(),
                timeout=max(0.01, wait_ms / 1000),
                return_when=asyncio.FIRST_COMPLETED,
            )
            for task in done:
                endpoint_id = active_tasks.pop(task)
                try:
                    _, transition = task.result()
                except Exception as exc:  # noqa: BLE001
                    scheduler.fail_probe(endpoint_id, now_ms=int(time.time() * 1000))
                    logger.warning("External IED TCP task failed | endpoint_id=%s error=%s", endpoint_id, exc)
                    continue
                if transition is None:
                    scheduler.fail_probe(endpoint_id, now_ms=int(time.time() * 1000))
                else:
                    scheduler.complete_probe(endpoint_id, transition)
        else:
            await asyncio.sleep(max(0.01, wait_ms / 1000))

    for task in active_tasks:
        task.cancel()
    if active_tasks:
        await asyncio.gather(*active_tasks.keys(), return_exceptions=True)
