from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.models.workspace import Workspace
from app.schemas.test_run_schema import TestRunCreateSchema, TestRunSchema
from app.services.signal_snapshot_service import SignalSnapshotNotFoundError
from app.services.test_run_service import (
    AllocationMissingError,
    TestRunNotFoundError,
    TestRunService,
    WorkspaceResourceNotFoundError,
)

router = APIRouter(prefix="/api/v1", tags=["TestRuns"])


async def _ensure_workspace(db: AsyncSession, workspace_id: int) -> None:
    stmt = select(Workspace.id).where(Workspace.id == workspace_id)
    result = await db.execute(stmt)
    if result.scalar_one_or_none() is None:
        raise HTTPException(status_code=404, detail="Workspace not found")


@router.post(
    "/workspaces/{workspace_id}/test-runs",
    response_model=TestRunSchema,
)
async def create_test_run(
    workspace_id: int,
    payload: TestRunCreateSchema,
    db: AsyncSession = Depends(get_db),
):
    await _ensure_workspace(db, workspace_id)
    service = TestRunService(db)
    try:
        run = await service.create_run(
            workspace_id=workspace_id,
            sequence_id=payload.sequence_id,
            snapshot_id=payload.signal_snapshot_id,
        )
    except WorkspaceResourceNotFoundError as exc:
        raise HTTPException(status_code=409, detail=str(exc))
    except SignalSnapshotNotFoundError:
        raise HTTPException(status_code=404, detail="Signal snapshot not found in workspace")
    except AllocationMissingError as exc:
        raise HTTPException(status_code=409, detail=str(exc))
    return run


@router.get(
    "/workspaces/{workspace_id}/test-runs",
    response_model=list[TestRunSchema],
)
async def list_test_runs(workspace_id: int, db: AsyncSession = Depends(get_db)):
    await _ensure_workspace(db, workspace_id)
    service = TestRunService(db)
    return await service.list_runs(workspace_id)


@router.get("/test-runs/{run_id}", response_model=TestRunSchema)
async def get_test_run(run_id: int, db: AsyncSession = Depends(get_db)):
    service = TestRunService(db)
    try:
        return await service.get_run(run_id)
    except TestRunNotFoundError:
        raise HTTPException(status_code=404, detail="Test run not found")


@router.post("/test-runs/{run_id}/repeat", response_model=TestRunSchema)
async def repeat_test_run(run_id: int, db: AsyncSession = Depends(get_db)):
    service = TestRunService(db)
    try:
        return await service.repeat_run(run_id)
    except TestRunNotFoundError:
        raise HTTPException(status_code=404, detail="Test run not found")
