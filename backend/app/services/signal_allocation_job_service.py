from __future__ import annotations

import json
from datetime import datetime, timezone
from typing import Any, Literal
from uuid import uuid4

from app.core.config import get_settings
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import enqueue_signal_allocation_job, enqueue_signal_test_run_job

settings = get_settings()

SignalAllocationJobStatus = Literal[
    "queued",
    "running",
    "paused",
    "cancelling",
    "cancelled",
    "succeeded",
    "failed",
]
SignalAllocationJobOperation = Literal["auto_allocate", "bulk_update", "test_run"]


def _job_key(job_id: str) -> str:
    return f"signal:allocation:job:{job_id}"


def _job_status_key(job_id: str) -> str:
    return f"signal:allocation:job:{job_id}:status"


def _test_run_lock_key(workspace_id: int) -> str:
    return f"signal:allocation:test-run:lock:{workspace_id}"


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


async def create_signal_allocation_job(
    *,
    workspace_id: int,
    operation: SignalAllocationJobOperation,
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

    snapshot = {
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
                lock_owner_snapshot = await get_signal_allocation_job(lock_owner_job_id)
                lock_owner_status = str((lock_owner_snapshot or {}).get("status") or "").strip().lower()
                stale_cancelling = False
                if lock_owner_status == "cancelling" and lock_owner_snapshot is not None:
                    updated_at_raw = str(lock_owner_snapshot.get("updated_at") or "").strip()
                    if updated_at_raw:
                        try:
                            updated_at = datetime.fromisoformat(updated_at_raw)
                            if updated_at.tzinfo is None:
                                updated_at = updated_at.replace(tzinfo=timezone.utc)
                            age_seconds = (datetime.now(timezone.utc) - updated_at).total_seconds()
                            stale_cancelling = age_seconds >= max(1, int(settings.signal_test_run_cancelling_stale_seconds))
                        except ValueError:
                            stale_cancelling = True
                    else:
                        stale_cancelling = True

                if lock_owner_snapshot is None or lock_owner_status in {"succeeded", "failed", "cancelled"} or stale_cancelling:
                    if stale_cancelling:
                        await update_signal_allocation_job(
                            lock_owner_job_id,
                            status="cancelled",
                            message="Cancelled (stale cancelling state)",
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
        json.dumps(snapshot, separators=(",", ":")),
        ex=settings.signal_allocation_job_ttl_seconds,
    )
    await redis.set(
        _job_status_key(job_id),
        snapshot["status"],
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

    return snapshot


async def release_test_run_workspace_lock(workspace_id: int, job_id: str) -> None:
    redis = RedisManager.get_instance()
    lock_key = _test_run_lock_key(workspace_id)
    lock_value = await redis.get(lock_key)
    if str(lock_value or "") != job_id:
        return
    await redis.delete(lock_key)


async def get_signal_allocation_job(job_id: str) -> dict[str, Any] | None:
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
    return _normalize_job_payload(payload)


async def get_signal_allocation_job_status(job_id: str) -> str | None:
    redis = RedisManager.get_instance()
    raw = await redis.get(_job_status_key(job_id))
    if raw is None:
        return None
    status = str(raw).strip()
    return status or None


async def update_signal_allocation_job(
    job_id: str,
    *,
    status: SignalAllocationJobStatus,
    message: str | None = None,
    error: str | None = None,
    progress_done: int | None = None,
    progress_total: int | None = None,
    result: dict[str, Any] | None = None,
) -> dict[str, Any] | None:
    redis = RedisManager.get_instance()
    current = await get_signal_allocation_job(job_id)
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


async def refresh_signal_allocation_job_ttl(
    job_id: str,
    *,
    workspace_id: int | None = None,
    include_test_run_lock: bool = False,
) -> None:
    redis = RedisManager.get_instance()
    ttl_seconds = max(1, int(settings.signal_allocation_job_ttl_seconds))
    await redis.expire(_job_key(job_id), ttl_seconds)
    await redis.expire(_job_status_key(job_id), ttl_seconds)

    if include_test_run_lock and workspace_id and workspace_id > 0:
        lock_key = _test_run_lock_key(workspace_id)
        lock_value = await redis.get(lock_key)
        if str(lock_value or "") == job_id:
            await redis.expire(lock_key, ttl_seconds)


async def control_signal_allocation_job(job_id: str, action: Literal["pause", "resume", "stop"]) -> dict[str, Any] | None:
    current = await get_signal_allocation_job(job_id)
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
        return await update_signal_allocation_job(
            job_id,
            status="paused",
            message="Paused",
        )

    if action == "resume":
        if operation != "test_run" or status != "paused":
            return current
        return await update_signal_allocation_job(
            job_id,
            status="running",
            message="Running",
        )

    if action == "stop":
        if status == "queued":
            updated = await update_signal_allocation_job(
                job_id,
                status="cancelled",
                message="Cancelled",
            )
            if updated and operation == "test_run":
                workspace_id = int(updated.get("workspace_id") or 0)
                if workspace_id > 0:
                    await release_test_run_workspace_lock(workspace_id, job_id)
            return updated
        if status in {"running", "paused", "cancelling"}:
            return await update_signal_allocation_job(
                job_id,
                status="cancelling",
                message="Cancelling",
            )

    return current
