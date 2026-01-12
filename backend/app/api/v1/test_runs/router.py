from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.models.workspace import Workspace
from app.schemas.sequence_run_schema import SequenceStateSchema
from app.schemas.test_run_schema import TestRunCreateSchema, TestRunSchema
from app.services.signal_snapshot_service import SignalSnapshotNotFoundError
from app.services.sequence_command_service import SequenceCommandService
from app.services.sequence_state_service import SequenceStateService
from app.services.test_run_service import (
    DuplicateSequenceSelectionError,
    TestRunNotFoundError,
    TestRunService,
    TestRunAllocationMissingError,
    TestRunSequencesMissingError,
    WorkspaceResourceNotFoundError,
)
from app.services.domain_errors import DomainError, TestRunInvalidStateError

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
    if payload.workspace_id != workspace_id:
        raise HTTPException(status_code=400, detail="workspace_id mismatch between path and payload")
    service = TestRunService(db)
    try:
        run = await service.create_run(payload)
    except DuplicateSequenceSelectionError as exc:
        raise HTTPException(status_code=400, detail=str(exc))
    except DomainError as exc:
        raise HTTPException(status_code=400, detail=str(exc))
    except WorkspaceResourceNotFoundError as exc:
        raise HTTPException(status_code=409, detail=str(exc))
    except SignalSnapshotNotFoundError:
        raise HTTPException(status_code=404, detail="Signal snapshot not found in workspace")
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


@router.post("/test-runs/{run_id}/start", response_model=list[SequenceStateSchema])
async def start_test_run(run_id: int, db: AsyncSession = Depends(get_db)):
    service = TestRunService(db)
    try:
        context = await service.build_execution_context(run_id)
    except TestRunNotFoundError:
        raise HTTPException(status_code=404, detail="Test run not found")
    except (TestRunAllocationMissingError, TestRunSequencesMissingError) as exc:
        raise HTTPException(status_code=409, detail=str(exc))

    try:
        await service.mark_running(run_id)
    except TestRunInvalidStateError as exc:
        raise HTTPException(status_code=409, detail=str(exc))

    await SequenceCommandService.enqueue_start(run_id)
    states: list[SequenceStateSchema] = []
    for sequence in context.sequences:
        states.append(await SequenceStateService.get_state(sequence.id))
    return states


@router.post("/test-runs/{run_id}/stop", response_model=list[SequenceStateSchema])
async def stop_test_run(run_id: int, db: AsyncSession = Depends(get_db)):
    service = TestRunService(db)
    try:
        context = await service.build_execution_context(run_id)
    except TestRunNotFoundError:
        raise HTTPException(status_code=404, detail="Test run not found")
    except (TestRunAllocationMissingError, TestRunSequencesMissingError) as exc:
        raise HTTPException(status_code=409, detail=str(exc))

    await SequenceCommandService.enqueue_stop(run_id)
    states: list[SequenceStateSchema] = []
    for sequence in context.sequences:
        states.append(await SequenceStateService.get_state(sequence.id))
    return states
