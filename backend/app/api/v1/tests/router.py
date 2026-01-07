from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.tests import TestRunRepository, TestRunStepRepository
from app.infrastructure.db.database import get_db
from app.models.test_run import TestRunStatus
from app.schemas.test_run_schema import (
    TestRunCreateSchema,
    TestRunReorderSchema,
    TestRunSchema,
    TestRunStateSchema,
    TestRunStepBulkCreateSchema,
    TestRunStepCreateSchema,
    TestRunStepSchema,
    TestRunStepUpdateSchema,
    TestRunSummarySchema,
    TestRunUpdateSchema,
)
from app.services.test_run_runner import (
    TestRunRunner,
    TestRunRunnerError,
    TestRunAlreadyRunningError,
    TestRunNotRunningError,
)


router = APIRouter(prefix="/api/v1/projects/{project_id}/tests", tags=["Tests"])


async def _get_run_or_404(repo: TestRunRepository, project_id: int, run_id: int):
    run = await repo.get(project_id, run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Test run not found")
    return run


def _ensure_mutable(status: TestRunStatus) -> None:
    if status == TestRunStatus.RUNNING:
        raise HTTPException(status_code=409, detail="Cannot modify steps while test run is running")


@router.get("", response_model=list[TestRunSummarySchema])
async def list_test_runs(project_id: int, db: AsyncSession = Depends(get_db)):
    repo = TestRunRepository(db)
    return await repo.list(project_id)


@router.post("", response_model=TestRunSchema)
async def create_test_run(
    project_id: int,
    payload: TestRunCreateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = TestRunRepository(db)
    data = payload.model_dump(exclude={"channel_ids", "settings"})
    try:
        run = await repo.create(
            project_id,
            data,
            payload.channel_ids,
            payload.settings.model_dump() if payload.settings else None,
        )
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    return run


@router.get("/{run_id}", response_model=TestRunSchema)
async def get_test_run(project_id: int, run_id: int, db: AsyncSession = Depends(get_db)):
    repo = TestRunRepository(db)
    return await _get_run_or_404(repo, project_id, run_id)


@router.patch("/{run_id}", response_model=TestRunSchema)
async def update_test_run(
    project_id: int,
    run_id: int,
    payload: TestRunUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = TestRunRepository(db)
    changes = payload.model_dump(exclude_unset=True)
    if "settings" in changes:
        changes["settings"] = payload.settings.model_dump() if payload.settings else None
    try:
        run = await repo.update(project_id, run_id, changes)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    if not run:
        raise HTTPException(status_code=404, detail="Test run not found")
    return run


@router.delete("/{run_id}")
async def delete_test_run(project_id: int, run_id: int, db: AsyncSession = Depends(get_db)):
    repo = TestRunRepository(db)
    deleted = await repo.delete(project_id, run_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Test run not found")
    return {"detail": "Test run deleted"}


@router.get("/{run_id}/steps", response_model=list[TestRunStepSchema])
async def list_steps(project_id: int, run_id: int, db: AsyncSession = Depends(get_db)):
    repo = TestRunRepository(db)
    await _get_run_or_404(repo, project_id, run_id)
    step_repo = TestRunStepRepository(db)
    return await step_repo.list_for_run(run_id)


@router.post("/{run_id}/steps", response_model=TestRunStepSchema)
async def create_step(
    project_id: int,
    run_id: int,
    payload: TestRunStepCreateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = TestRunRepository(db)
    run = await _get_run_or_404(repo, project_id, run_id)
    _ensure_mutable(run.status)
    step_repo = TestRunStepRepository(db)
    try:
        return await step_repo.create(run_id, payload.channel_id)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc


@router.post("/{run_id}/steps/bulk", response_model=list[TestRunStepSchema])
async def bulk_add_steps(
    project_id: int,
    run_id: int,
    payload: TestRunStepBulkCreateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = TestRunRepository(db)
    run = await _get_run_or_404(repo, project_id, run_id)
    _ensure_mutable(run.status)
    step_repo = TestRunStepRepository(db)
    try:
        return await step_repo.bulk_create(run_id, payload.channel_ids)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc


@router.patch("/{run_id}/steps/{step_id}", response_model=TestRunStepSchema)
async def update_step(
    project_id: int,
    run_id: int,
    step_id: int,
    payload: TestRunStepUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = TestRunRepository(db)
    run = await _get_run_or_404(repo, project_id, run_id)
    _ensure_mutable(run.status)
    step_repo = TestRunStepRepository(db)
    try:
        step = await step_repo.update(step_id, payload.model_dump(exclude_unset=True))
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    if not step:
        raise HTTPException(status_code=404, detail="Test run step not found")
    return step


@router.delete("/{run_id}/steps/{step_id}")
async def delete_step(project_id: int, run_id: int, step_id: int, db: AsyncSession = Depends(get_db)):
    repo = TestRunRepository(db)
    run = await _get_run_or_404(repo, project_id, run_id)
    _ensure_mutable(run.status)
    step_repo = TestRunStepRepository(db)
    deleted = await step_repo.delete(step_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Test run step not found")
    return {"detail": "Test run step deleted"}


@router.post("/{run_id}/steps/reorder", response_model=list[TestRunStepSchema])
async def reorder_steps(
    project_id: int,
    run_id: int,
    payload: TestRunReorderSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = TestRunRepository(db)
    run = await _get_run_or_404(repo, project_id, run_id)
    _ensure_mutable(run.status)
    step_repo = TestRunStepRepository(db)
    return await step_repo.reorder(run_id, payload.new_order)


@router.post("/{run_id}/start", response_model=TestRunSchema)
async def start_test_run(project_id: int, run_id: int, db: AsyncSession = Depends(get_db)):
    repo = TestRunRepository(db)
    await _get_run_or_404(repo, project_id, run_id)
    runner = TestRunRunner.get_instance()
    try:
        await runner.start(project_id, run_id)
    except TestRunAlreadyRunningError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
    except TestRunRunnerError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    return await _get_run_or_404(repo, project_id, run_id)


@router.post("/{run_id}/cancel", response_model=TestRunSchema)
async def cancel_test_run(project_id: int, run_id: int, db: AsyncSession = Depends(get_db)):
    repo = TestRunRepository(db)
    await _get_run_or_404(repo, project_id, run_id)
    runner = TestRunRunner.get_instance()
    try:
        await runner.cancel(run_id)
    except TestRunNotRunningError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
    return await _get_run_or_404(repo, project_id, run_id)


@router.get("/{run_id}/state", response_model=TestRunStateSchema)
async def get_test_run_state(project_id: int, run_id: int, db: AsyncSession = Depends(get_db)):
    repo = TestRunRepository(db)
    run = await _get_run_or_404(repo, project_id, run_id)
    total_steps = len(run.steps)
    return TestRunStateSchema(
        id=run.id,
        status=run.status.value if isinstance(run.status, TestRunStatus) else run.status,
        started_at=run.started_at,
        finished_at=run.finished_at,
        current_step_index=run.current_step_index,
        total_steps=total_steps,
        error_message=run.error_message,
    )
