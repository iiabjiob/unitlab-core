from __future__ import annotations

from datetime import datetime
from typing import Literal

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from app.services.worker_health import SystemHealthSnapshot, WorkerHealth, get_cached_system_snapshot

router = APIRouter(prefix="/api/v1", tags=["Health"])


class WorkerHealthResponse(BaseModel):
    name: str
    display_name: str
    status: Literal["online", "degraded", "offline", "unknown"]
    last_seen_at: datetime | None = None
    impact: str
    detail: str | None = None


class SystemHealthResponse(BaseModel):
    status: Literal["online", "degraded", "offline"]
    checked_at: datetime
    issues: list[str]
    workers: list[WorkerHealthResponse]


def _worker_to_response(worker: WorkerHealth) -> WorkerHealthResponse:
    return WorkerHealthResponse(
        name=worker.name,
        display_name=worker.display_name,
        status=worker.status,
        last_seen_at=worker.last_seen_at,
        impact=worker.impact,
        detail=worker.detail,
    )


def _snapshot_to_response(snapshot: SystemHealthSnapshot) -> SystemHealthResponse:
    return SystemHealthResponse(
        status=snapshot.status,
        checked_at=snapshot.checked_at,
        issues=list(snapshot.issues),
        workers=[_worker_to_response(worker) for worker in snapshot.workers],
    )


@router.get("/health", response_model=SystemHealthResponse)
async def health() -> SystemHealthResponse:
    snapshot = await get_cached_system_snapshot()
    if not snapshot:
        raise HTTPException(status_code=503, detail="Health snapshot unavailable")
    return _snapshot_to_response(snapshot)
