from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from fastapi.responses import PlainTextResponse
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.test_runs import TestRunsRepository
from app.api.v1.test_runs.serializer import to_test_run_record, to_test_run_signal_snapshot
from app.infrastructure.db.database import get_db
from app.models.test_run import TestRunStatus
from app.schemas.sequence_run_schema import SequenceStateSchema
from app.schemas.test_run_schema import (
    TestRunCreateSchema,
    TestRunPreflightSchema,
    TestRunReallocateSchema,
    TestRunRecordSchema,
    TestRunSignalSnapshotSchema,
)
from app.services.cable_journal_service import CableJournalService
from app.services.test_run_orchestrator import TestRunConflictError, TestRunOrchestrator
from app.services.test_run_preflight_service import TestRunPreflightService
from app.services.test_run_state_service import TestRunStateService

router = APIRouter(prefix="/api/v1", tags=["Test Runs"])


def get_repo(db: AsyncSession = Depends(get_db)) -> TestRunsRepository:
    return TestRunsRepository(db)


def get_orchestrator(repo: TestRunsRepository = Depends(get_repo)) -> TestRunOrchestrator:
    return TestRunOrchestrator(repo)


@router.get("/workspaces/{workspace_id}/test-runs", response_model=list[TestRunRecordSchema])
async def list_test_runs(
    workspace_id: int,
    repo: TestRunsRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    runs = await repo.list(workspace_id)
    for run in runs:
        await TestRunStateService.sync_status(repo.db, run)
    await repo.db.commit()

    return [to_test_run_record(run) for run in runs]


@router.post("/workspaces/{workspace_id}/test-runs", response_model=TestRunRecordSchema)
async def create_test_run(
    workspace_id: int,
    payload: TestRunCreateSchema,
    repo: TestRunsRepository = Depends(get_repo),
):
    if payload.workspace_id != workspace_id:
        raise HTTPException(status_code=400, detail="workspace_id mismatch")

    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    sequence_ids = _normalize_sequence_ids(payload.sequence_ids)
    if not sequence_ids:
        raise HTTPException(status_code=400, detail="At least one sequence is required")
    if not await repo.ensure_sequences_in_workspace(workspace_id, sequence_ids):
        raise HTTPException(status_code=400, detail="Unknown sequence(s) for workspace")

    allocation_entries = payload.allocation.entries if payload.allocation else []
    if not payload.allow_empty_allocation and not allocation_entries:
        raise HTTPException(status_code=400, detail="Allocation is required unless allow_empty_allocation is true")

    channel_ids = [entry.channel_id for entry in allocation_entries]
    if not await repo.ensure_channels_exist(channel_ids):
        raise HTTPException(status_code=400, detail="Unknown channel(s) in allocation")

    signal_ids = [entry.signal_id for entry in allocation_entries if entry.signal_id is not None]
    if not await repo.ensure_signals_in_workspace(workspace_id, signal_ids):
        raise HTTPException(status_code=400, detail="Unknown signal(s) in allocation")

    allocation_notes = payload.allocation.notes if payload.allocation else None
    entries_payload = [entry.model_dump() for entry in allocation_entries]

    run = await repo.create(
        workspace_id=workspace_id,
        sequence_ids=sequence_ids,
        allocation_notes=allocation_notes,
        allocation_entries=entries_payload,
    )

    return to_test_run_record(run)


@router.get("/test-runs/{run_id}", response_model=TestRunRecordSchema)
async def get_test_run(run_id: int, repo: TestRunsRepository = Depends(get_repo)):
    run = await repo.get(run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Test run not found")

    await TestRunStateService.sync_status(repo.db, run)
    await repo.db.commit()

    return to_test_run_record(run)


@router.post("/test-runs/{run_id}/repeat", response_model=TestRunRecordSchema)
async def repeat_test_run(run_id: int, repo: TestRunsRepository = Depends(get_repo)):
    run = await repo.get(run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Test run not found")

    clone = await repo.clone(run)
    return to_test_run_record(clone)


@router.get("/test-runs/{run_id}/preflight", response_model=TestRunPreflightSchema)
async def preflight_test_run(run_id: int, repo: TestRunsRepository = Depends(get_repo)):
    run = await repo.get(run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Test run not found")
    return await TestRunPreflightService.evaluate(repo, run)


@router.post("/test-runs/{run_id}/reallocate", response_model=TestRunRecordSchema)
async def reallocate_test_run(
    run_id: int,
    payload: TestRunReallocateSchema,
    repo: TestRunsRepository = Depends(get_repo),
):
    source = await repo.get(run_id)
    if not source:
        raise HTTPException(status_code=404, detail="Test run not found")

    if not source.allocation:
        raise HTTPException(status_code=400, detail="Test run has no allocation to reassign")

    entry_by_id = {entry.id: entry for entry in source.allocation.entries}
    overrides: dict[int, int] = {}
    for item in payload.reallocation:
        if item.allocation_entry_id not in entry_by_id:
            raise HTTPException(
                status_code=400,
                detail=f"Allocation entry #{item.allocation_entry_id} does not belong to test run",
            )
        overrides[item.allocation_entry_id] = item.channel_id

    channel_ids = {entry.channel_id for entry in source.allocation.entries}
    channel_ids.update(overrides.values())
    channels = await repo.get_channels_by_ids(channel_ids)
    if set(channels.keys()) != channel_ids:
        raise HTTPException(status_code=400, detail="One or more reassigned channels do not exist")

    new_entries: list[dict] = []
    for entry in source.allocation.entries:
        next_channel_id = overrides.get(entry.id, entry.channel_id)
        candidate_channel = channels.get(next_channel_id)
        if candidate_channel is None:
            raise HTTPException(status_code=400, detail=f"Channel #{next_channel_id} not found")

        # If signal direction is known, ensure channel family matches before creating a new run revision.
        if entry.signal is not None:
            direction_value = entry.signal.io_direction
            signal_direction = direction_value if isinstance(direction_value, str) else direction_value.value
            if signal_direction[:2].lower() != str(candidate_channel.channel_type).strip().lower()[:2]:
                raise HTTPException(
                    status_code=400,
                    detail=(
                        f"Channel #{next_channel_id} is incompatible with signal direction {signal_direction}"
                    ),
                )

        metadata = dict(entry.signal_metadata or {})
        metadata["reallocated_from_channel_id"] = entry.channel_id if next_channel_id != entry.channel_id else None
        new_entries.append(
            {
                "channel_id": next_channel_id,
                "signal_id": entry.signal_id,
                "signal_metadata": metadata,
            }
        )

    sequence_ids = [link.sequence_id for link in sorted(source.sequence_links, key=lambda item: item.order_index)]
    notes = payload.notes if payload.notes is not None else source.allocation.notes
    clone = await repo.create(
        workspace_id=source.workspace_id,
        sequence_ids=sequence_ids,
        allocation_notes=notes,
        allocation_entries=new_entries,
        source_test_run_id=source.id,
        allocation_revision=int(source.allocation_revision or 1) + 1,
    )

    return to_test_run_record(clone)


@router.get("/test-runs/{run_id}/signals", response_model=TestRunSignalSnapshotSchema)
async def get_test_run_signal_snapshot(run_id: int, repo: TestRunsRepository = Depends(get_repo)):
    run = await repo.get(run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Test run not found")

    snapshot = await repo.get_signal_snapshot(run_id)
    if not snapshot:
        raise HTTPException(status_code=404, detail="Signal snapshot not captured yet")

    return to_test_run_signal_snapshot(run_id, snapshot)


@router.get("/test-runs/{run_id}/cable-journal/export")
async def export_test_run_cable_journal(run_id: int, repo: TestRunsRepository = Depends(get_repo)):
    run = await repo.get(run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Test run not found")

    csv_body = CableJournalService.export_csv(run)
    return PlainTextResponse(
        csv_body,
        media_type="text/csv",
        headers={"Content-Disposition": f'attachment; filename="test-run-{run_id}-cable-journal.csv"'},
    )


@router.post("/test-runs/{run_id}/start", response_model=list[SequenceStateSchema])
async def start_test_run(
    run_id: int,
    repo: TestRunsRepository = Depends(get_repo),
    orchestrator: TestRunOrchestrator = Depends(get_orchestrator),
):
    run = await repo.get(run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Test run not found")

    preflight = await TestRunPreflightService.evaluate(repo, run)
    if not preflight.ready:
        raise HTTPException(
            status_code=409,
            detail={
                "message": "Preflight failed. Reallocate unavailable channels before start.",
                "preflight": preflight.model_dump(),
            },
        )

    try:
        states = await orchestrator.start(run)
    except TestRunConflictError as exc:
        raise HTTPException(status_code=409, detail=str(exc))

    return states


@router.post("/test-runs/{run_id}/stop", response_model=list[SequenceStateSchema])
async def stop_test_run(
    run_id: int,
    repo: TestRunsRepository = Depends(get_repo),
    orchestrator: TestRunOrchestrator = Depends(get_orchestrator),
):
    run = await repo.get(run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Test run not found")
    if run.status != TestRunStatus.RUNNING:
        raise HTTPException(status_code=409, detail="Test run is not running")

    try:
        states = await orchestrator.stop(run)
    except TestRunConflictError as exc:
        raise HTTPException(status_code=409, detail=str(exc))

    return states


def _normalize_sequence_ids(raw_ids: list[int]) -> list[int]:
    seen: set[int] = set()
    sequence_ids: list[int] = []
    for raw in raw_ids:
        value = int(raw)
        if value in seen:
            continue
        seen.add(value)
        sequence_ids.append(value)
    return sequence_ids
