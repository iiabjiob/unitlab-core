from __future__ import annotations

import json
from datetime import datetime, timezone
from typing import Any, Literal
from uuid import uuid4

from app.core.config import get_settings
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import enqueue_signal_allocation_job

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

    await redis.set(
        _job_key(job_id),
        json.dumps(snapshot, separators=(",", ":")),
        ex=settings.signal_allocation_job_ttl_seconds,
    )

    await enqueue_signal_allocation_job(
        {
            "job_id": job_id,
            "workspace_id": workspace_id,
            "operation": operation,
            "payload": payload,
            "created_at": now_iso,
        }
    )

    return snapshot


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
    return next_payload


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
            return await update_signal_allocation_job(
                job_id,
                status="cancelled",
                message="Cancelled",
            )
        if status in {"running", "paused", "cancelling"}:
            return await update_signal_allocation_job(
                job_id,
                status="cancelling",
                message="Cancelling",
            )

    return current
