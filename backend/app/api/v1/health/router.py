from __future__ import annotations

from datetime import datetime
from typing import Literal

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from app.services.signal_job_service import (
    get_signal_test_run_execution_lease_stats,
    reset_signal_test_run_execution_lease_stats,
)
from app.services.worker_health import SystemHealthSnapshot, WorkerHealth, get_cached_system_snapshot
from app.ws.manager import WebSocketManager

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


class WsRuntimeStatsResponse(BaseModel):
    active_connections: int
    outbound_queues: int
    sender_tasks: int
    sync_tasks: int
    counters: dict[str, int]


class SignalTestRunLeaseStatsResponse(BaseModel):
    updated_at: str | None = None
    counters: dict[str, int]


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


@router.get("/health/ws", response_model=WsRuntimeStatsResponse)
async def health_ws() -> WsRuntimeStatsResponse:
    manager = WebSocketManager.get_instance()
    stats = manager.get_stats()
    return WsRuntimeStatsResponse.model_validate(stats)


@router.post("/health/ws/reset", response_model=WsRuntimeStatsResponse)
async def health_ws_reset() -> WsRuntimeStatsResponse:
    manager = WebSocketManager.get_instance()
    stats = manager.reset_stats()
    return WsRuntimeStatsResponse.model_validate(stats)


@router.get("/health/signal-test-run/lease", response_model=SignalTestRunLeaseStatsResponse)
async def health_signal_test_run_lease_stats() -> SignalTestRunLeaseStatsResponse:
    stats = await get_signal_test_run_execution_lease_stats()
    return SignalTestRunLeaseStatsResponse.model_validate(stats)


@router.post("/health/signal-test-run/lease/reset", response_model=SignalTestRunLeaseStatsResponse)
async def health_signal_test_run_lease_stats_reset() -> SignalTestRunLeaseStatsResponse:
    stats = await reset_signal_test_run_execution_lease_stats()
    return SignalTestRunLeaseStatsResponse.model_validate(stats)
