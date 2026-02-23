from __future__ import annotations

import json
from datetime import datetime, timezone
from typing import Any, Literal
from uuid import uuid4

from app.core.config import get_settings
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import enqueue_signal_allocation_job, enqueue_signal_test_run_job

settings = get_settings()

SignalJobStatus = Literal[
    "queued",
    "running",
    "paused",
    "cancelling",
    "cancelled",
    "succeeded",
    "failed",
]
SignalJobOperation = Literal["auto_allocate", "bulk_update", "test_run"]


def _job_key(job_id: str) -> str:
    return f"signal:allocation:job:{job_id}"


def _job_status_key(job_id: str) -> str:
    return f"signal:allocation:job:{job_id}:status"


def _job_cursor_key(job_id: str) -> str:
    return f"signal:allocation:job:{job_id}:cursor"


def _test_run_lock_key(workspace_id: int) -> str:
    return f"signal:allocation:test-run:lock:{workspace_id}"


def _test_run_execution_lease_key(job_id: str) -> str:
    return f"signal:allocation:test-run:exec:{job_id}"


def _test_run_execution_lease_stats_key() -> str:
    return "signal:allocation:test-run:exec:lease-stats"


def _now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def _normalize_job_payload(payload: dict[str, Any]) -> dict[str, Any]:
    result = dict(payload)
    result.setdefault("progress_total", 0)
    result.setdefault("progress_done", 0)
    result.setdefault("message", None)
    result.setdefault("error", None)
    result.setdefault("result", {})
    result.setdefault("created_at", _now_iso())
    result.setdefault("updated_at", result["created_at"])
    return result


async def create_signal_job(
    *,
    workspace_id: int,
    operation: SignalJobOperation,
    payload: dict[str, Any],
) -> dict[str, Any]:
    redis = RedisManager.get_instance()
    job_id = uuid4().hex
    now_iso = _now_iso()
    lock_key = _test_run_lock_key(workspace_id) if operation == "test_run" else None

    progress_total = 0
    if operation == "auto_allocate":
        signal_ids = payload.get("signal_ids") if isinstance(payload, dict) else None
        if isinstance(signal_ids, list):
            progress_total = len(signal_ids)
    elif operation == "bulk_update":
        entries = payload.get("entries") if isinstance(payload, dict) else None
        if isinstance(entries, list):
            progress_total = len(entries)
    elif operation == "test_run":
        signal_ids = payload.get("signal_ids") if isinstance(payload, dict) else None
        if isinstance(signal_ids, list):
            progress_total = len(signal_ids)

    job_state = {
        "job_id": job_id,
        "workspace_id": workspace_id,
        "operation": operation,
        "status": "queued",
        "progress_total": progress_total,
        "progress_done": 0,
        "message": "Queued",
        "error": None,
        "result": {},
        "created_at": now_iso,
        "updated_at": now_iso,
    }

    if lock_key is not None:
        lock_owner_job_id = ""
        lock_owner_status = ""
        acquired = await redis.set(
            lock_key,
            job_id,
            ex=settings.signal_allocation_job_ttl_seconds,
            nx=True,
        )
        if not acquired:
            lock_owner_job_id = str(await redis.get(lock_key) or "").strip()
            if not lock_owner_job_id:
                await redis.delete(lock_key)
                acquired = await redis.set(
                    lock_key,
                    job_id,
                    ex=settings.signal_allocation_job_ttl_seconds,
                    nx=True,
                )
            else:
                lock_owner_state = await get_signal_job(lock_owner_job_id)
                lock_owner_status = str((lock_owner_state or {}).get("status") or "").strip().lower()
                stale_non_terminal = False
                if lock_owner_state is not None and lock_owner_status in {"queued", "running", "paused", "cancelling"}:
                    updated_at_raw = str(lock_owner_state.get("updated_at") or "").strip()
                    if updated_at_raw:
                        try:
                            updated_at = datetime.fromisoformat(updated_at_raw)
                            if updated_at.tzinfo is None:
                                updated_at = updated_at.replace(tzinfo=timezone.utc)
                            age_seconds = (datetime.now(timezone.utc) - updated_at).total_seconds()
                            stale_non_terminal = age_seconds >= max(1, int(settings.signal_test_run_cancelling_stale_seconds))
                        except ValueError:
                            stale_non_terminal = True
                    else:
                        stale_non_terminal = True

                if lock_owner_state is None or lock_owner_status in {"succeeded", "failed", "cancelled"} or stale_non_terminal:
                    if stale_non_terminal:
                        await update_signal_job(
                            lock_owner_job_id,
                            status="cancelled",
                            message="Cancelled (stale test-run lock)",
                        )
                    await redis.delete(lock_key)
                    acquired = await redis.set(
                        lock_key,
                        job_id,
                        ex=settings.signal_allocation_job_ttl_seconds,
                        nx=True,
                    )
        if not acquired:
            suffix = f" (job_id={lock_owner_job_id or 'unknown'}, status={lock_owner_status or 'unknown'})"
            raise RuntimeError(f"Another test run is already active for this workspace{suffix}")

    await redis.set(
        _job_key(job_id),
        json.dumps(job_state, separators=(",", ":")),
        ex=settings.signal_allocation_job_ttl_seconds,
    )
    await redis.set(
        _job_status_key(job_id),
        job_state["status"],
        ex=settings.signal_allocation_job_ttl_seconds,
    )

    envelope = {
        "job_id": job_id,
        "workspace_id": workspace_id,
        "operation": operation,
        "payload": payload,
        "created_at": now_iso,
    }
    try:
        if operation == "test_run":
            await enqueue_signal_test_run_job(envelope)
        else:
            await enqueue_signal_allocation_job(envelope)
    except Exception:
        await redis.delete(_job_key(job_id))
        if lock_key is not None:
            await redis.delete(lock_key)
        raise

    return job_state


async def release_signal_test_run_workspace_lock(workspace_id: int, job_id: str) -> None:
    redis = RedisManager.get_instance()
    lock_key = _test_run_lock_key(workspace_id)
    lock_value = await redis.get(lock_key)
    if str(lock_value or "") != job_id:
        return
    await redis.delete(lock_key)


def _signal_test_run_execution_lease_ttl_seconds() -> int:
    refresh_seconds = max(1, int(settings.signal_test_run_ttl_refresh_seconds))
    return max(refresh_seconds * 3, refresh_seconds + 5)


async def acquire_signal_test_run_execution_lease(*, job_id: str, owner: str) -> bool:
    redis = RedisManager.get_instance()
    acquired = await redis.set(
        _test_run_execution_lease_key(job_id),
        owner,
        ex=_signal_test_run_execution_lease_ttl_seconds(),
        nx=True,
    )
    return bool(acquired)


async def refresh_signal_test_run_execution_lease(*, job_id: str, owner: str) -> bool:
    redis = RedisManager.get_instance()
    key = _test_run_execution_lease_key(job_id)
    ttl_seconds = _signal_test_run_execution_lease_ttl_seconds()
    # Refresh only if the same worker still owns the lease.
    script = """
    local v = redis.call('GET', KEYS[1])
    if not v then
      return 0
    end
    if v ~= ARGV[1] then
      return 0
    end
    redis.call('EXPIRE', KEYS[1], tonumber(ARGV[2]))
    return 1
    """
    result = await redis.eval(script, 1, key, owner, str(ttl_seconds))
    return bool(int(result or 0))


async def release_signal_test_run_execution_lease(*, job_id: str, owner: str) -> bool:
    redis = RedisManager.get_instance()
    key = _test_run_execution_lease_key(job_id)
    # Release only if the same worker still owns the lease.
    script = """
    local v = redis.call('GET', KEYS[1])
    if not v then
      return 0
    end
    if v ~= ARGV[1] then
      return 0
    end
    redis.call('DEL', KEYS[1])
    return 1
    """
    result = await redis.eval(script, 1, key, owner)
    return bool(int(result or 0))


async def increment_signal_test_run_execution_lease_stat(name: str, delta: int = 1) -> int:
    redis = RedisManager.get_instance()
    key = _test_run_execution_lease_stats_key()
    value = await redis.hincrby(key, name, int(delta))
    await redis.hset(key, mapping={"updated_at": _now_iso()})
    return int(value)


async def get_signal_test_run_execution_lease_stats() -> dict[str, Any]:
    redis = RedisManager.get_instance()
    raw = await redis.hgetall(_test_run_execution_lease_stats_key())
    counters: dict[str, int] = {}
    updated_at = None
    for key, value in (raw or {}).items():
        if key == "updated_at":
            updated_at = str(value)
            continue
        try:
            counters[str(key)] = int(value)
        except (TypeError, ValueError):
            continue
    return {
        "updated_at": updated_at,
        "counters": counters,
    }


async def reset_signal_test_run_execution_lease_stats() -> dict[str, Any]:
    redis = RedisManager.get_instance()
    key = _test_run_execution_lease_stats_key()
    await redis.delete(key)
    return await get_signal_test_run_execution_lease_stats()


async def get_signal_job(job_id: str) -> dict[str, Any] | None:
    redis = RedisManager.get_instance()
    raw = await redis.get(_job_key(job_id))
    if not raw:
        return None
    try:
        payload = json.loads(raw)
    except json.JSONDecodeError:
        return None
    if not isinstance(payload, dict):
        return None
    normalized = _normalize_job_payload(payload)
    cursor = await get_signal_job_progress_cursor(job_id)
    if cursor is not None:
        normalized["progress_cursor"] = cursor
    return normalized


async def get_signal_job_status(job_id: str) -> str | None:
    redis = RedisManager.get_instance()
    raw = await redis.get(_job_status_key(job_id))
    if raw is None:
        return None
    status = str(raw).strip()
    return status or None


async def update_signal_job(
    job_id: str,
    *,
    status: SignalJobStatus,
    message: str | None = None,
    error: str | None = None,
    progress_done: int | None = None,
    progress_total: int | None = None,
    result: dict[str, Any] | None = None,
) -> dict[str, Any] | None:
    redis = RedisManager.get_instance()
    current = await get_signal_job(job_id)
    if current is None:
        return None

    next_payload = dict(current)
    next_payload["status"] = status
    next_payload["updated_at"] = _now_iso()
    if message is not None:
        next_payload["message"] = message
    if error is not None:
        next_payload["error"] = error
    if progress_done is not None:
        next_payload["progress_done"] = max(0, int(progress_done))
    if progress_total is not None:
        next_payload["progress_total"] = max(0, int(progress_total))
    if result is not None:
        next_payload["result"] = result

    await redis.set(
        _job_key(job_id),
        json.dumps(next_payload, separators=(",", ":")),
        ex=settings.signal_allocation_job_ttl_seconds,
    )
    await redis.set(
        _job_status_key(job_id),
        next_payload["status"],
        ex=settings.signal_allocation_job_ttl_seconds,
    )
    return next_payload


async def set_signal_job_progress_cursor(job_id: str, cursor: dict[str, Any]) -> None:
    redis = RedisManager.get_instance()
    payload = dict(cursor)
    payload.setdefault("updated_at", _now_iso())
    await redis.set(
        _job_cursor_key(job_id),
        json.dumps(payload, separators=(",", ":")),
        ex=settings.signal_allocation_job_ttl_seconds,
    )


async def get_signal_job_progress_cursor(job_id: str) -> dict[str, Any] | None:
    redis = RedisManager.get_instance()
    raw = await redis.get(_job_cursor_key(job_id))
    if not raw:
        return None
    try:
        payload = json.loads(raw)
    except json.JSONDecodeError:
        return None
    if not isinstance(payload, dict):
        return None
    return payload


async def refresh_signal_job_ttl(
    job_id: str,
    *,
    workspace_id: int | None = None,
    include_test_run_lock: bool = False,
) -> None:
    redis = RedisManager.get_instance()
    ttl_seconds = max(1, int(settings.signal_allocation_job_ttl_seconds))
    await redis.expire(_job_key(job_id), ttl_seconds)
    await redis.expire(_job_status_key(job_id), ttl_seconds)
    await redis.expire(_job_cursor_key(job_id), ttl_seconds)

    if include_test_run_lock and workspace_id and workspace_id > 0:
        lock_key = _test_run_lock_key(workspace_id)
        lock_value = await redis.get(lock_key)
        if str(lock_value or "") == job_id:
            await redis.expire(lock_key, ttl_seconds)


async def control_signal_job(job_id: str, action: Literal["pause", "resume", "stop"]) -> dict[str, Any] | None:
    current = await get_signal_job(job_id)
    if current is None:
        return None

    status = str(current.get("status") or "")
    operation = str(current.get("operation") or "")
    terminal = {"succeeded", "failed", "cancelled"}
    if status in terminal:
        return current

    if action == "pause":
        if operation != "test_run" or status != "running":
            return current
        return await update_signal_job(
            job_id,
            status="paused",
            message="Paused",
        )

    if action == "resume":
        if operation != "test_run" or status != "paused":
            return current
        return await update_signal_job(
            job_id,
            status="running",
            message="Running",
        )

    if action == "stop":
        if status == "queued":
            updated = await update_signal_job(
                job_id,
                status="cancelled",
                message="Cancelled",
            )
            if updated and operation == "test_run":
                workspace_id = int(updated.get("workspace_id") or 0)
                if workspace_id > 0:
                    await release_signal_test_run_workspace_lock(workspace_id, job_id)
            return updated
        if status == "paused":
            updated = await update_signal_job(
                job_id,
                status="cancelled",
                message="Cancelled",
            )
            if updated and operation == "test_run":
                workspace_id = int(updated.get("workspace_id") or 0)
                if workspace_id > 0:
                    await release_signal_test_run_workspace_lock(workspace_id, job_id)
            return updated
        if status in {"running", "paused", "cancelling"}:
            return await update_signal_job(
                job_id,
                status="cancelling",
                message="Cancelling",
            )

    return current


# Backward-compatible aliases (temporary)
SignalAllocationJobStatus = SignalJobStatus
SignalAllocationJobOperation = SignalJobOperation
create_signal_allocation_job = create_signal_job
get_signal_allocation_job = get_signal_job
get_signal_allocation_job_status = get_signal_job_status
update_signal_allocation_job = update_signal_job
refresh_signal_allocation_job_ttl = refresh_signal_job_ttl
control_signal_allocation_job = control_signal_job
release_test_run_workspace_lock = release_signal_test_run_workspace_lock
acquire_test_run_execution_lease = acquire_signal_test_run_execution_lease
refresh_test_run_execution_lease = refresh_signal_test_run_execution_lease
release_test_run_execution_lease = release_signal_test_run_execution_lease
