"""REST API for managing sequences, steps, and runtime state."""
from __future__ import annotations

import json
import re
from typing import List

from fastapi import APIRouter, Depends, File, HTTPException, UploadFile
from fastapi.responses import JSONResponse
from pydantic import ValidationError
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.models.channel import Channel
from app.models.device import Device
from app.api.v1.sequences import SequenceRepository, SequenceStepRepository
from app.schemas.sequence_run_schema import SequenceStateSchema
from app.schemas.sequence_schema import (
    SequenceCreateSchema,
    SequenceExportSchema,
    SequenceSchema,
    SequenceUpdateSchema,
)
from app.schemas.sequence_step_schema import (
    SequenceReorderSchema,
    SequenceStepCreateSchema,
    SequenceStepSchema,
    SequenceStepUpdateSchema,
)
from app.services.sequence_command_service import SequenceCommandService
from app.services.sequence_runner import SequenceNotFoundError
from app.services.sequence_state_service import SequenceStateService

router = APIRouter(prefix="/api/v1/workspaces/{workspace_id}/sequences", tags=["Sequences"])


async def _ensure_sequence_in_workspace(
    repo: SequenceRepository, workspace_id: int, seq_id: int
) -> None:
    if not await repo.ensure(workspace_id, seq_id):
        raise HTTPException(status_code=404, detail="Sequence not found")


# ---------------------------------------------------------------------------
# CRUD
# ---------------------------------------------------------------------------
@router.get("", response_model=list[SequenceSchema])
async def list_sequences(workspace_id: int, db: AsyncSession = Depends(get_db)):
    repo = SequenceRepository(db)
    return await repo.list(workspace_id)


@router.get("/{seq_id}", response_model=SequenceSchema)
async def get_sequence(workspace_id: int, seq_id: int, db: AsyncSession = Depends(get_db)):
    repo = SequenceRepository(db)
    seq = await repo.get(workspace_id, seq_id)
    if not seq:
        raise HTTPException(status_code=404, detail="Sequence not found")
    return seq


@router.post("", response_model=SequenceSchema)
async def create_sequence(
    workspace_id: int,
    payload: SequenceCreateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = SequenceRepository(db)
    data = payload.model_dump(exclude={"steps"})
    steps = [step.model_dump() for step in payload.steps]
    return await repo.create(workspace_id, data, steps)


@router.patch("/{seq_id}", response_model=SequenceSchema)
async def update_sequence(
    workspace_id: int,
    seq_id: int,
    payload: SequenceUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = SequenceRepository(db)
    seq = await repo.update(workspace_id, seq_id, payload.model_dump(exclude_unset=True))
    if not seq:
        raise HTTPException(status_code=404, detail="Sequence not found")
    return seq


@router.delete("/{seq_id}")
async def delete_sequence(workspace_id: int, seq_id: int, db: AsyncSession = Depends(get_db)):
    repo = SequenceRepository(db)
    deleted = await repo.delete(workspace_id, seq_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Sequence not found")
    return {"detail": "Sequence deleted"}


# ---------------------------------------------------------------------------
# Steps
# ---------------------------------------------------------------------------
@router.get("/{seq_id}/steps", response_model=list[SequenceStepSchema])
async def list_steps(workspace_id: int, seq_id: int, db: AsyncSession = Depends(get_db)):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    return await repo.get_for_sequence(seq_id)


@router.post("/{seq_id}/steps", response_model=SequenceStepSchema)
async def create_step(
    workspace_id: int,
    seq_id: int,
    payload: SequenceStepCreateSchema,
    db: AsyncSession = Depends(get_db),
):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    step = await repo.create(seq_id, payload.model_dump(exclude_unset=True))
    return step


@router.patch("/{seq_id}/steps/{step_id}", response_model=SequenceStepSchema)
async def update_step(
    workspace_id: int,
    seq_id: int,
    step_id: int,
    payload: SequenceStepUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    step = await repo.update(step_id, payload.model_dump(exclude_unset=True))
    if not step:
        raise HTTPException(status_code=404, detail="Step not found")
    return step


@router.delete("/{seq_id}/steps/{step_id}")
async def delete_step(workspace_id: int, seq_id: int, step_id: int, db: AsyncSession = Depends(get_db)):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    deleted = await repo.delete(step_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Step not found")

    await repo.normalize(seq_id)
    return {"detail": "Step deleted"}


@router.post("/{seq_id}/steps/reorder", response_model=list[SequenceStepSchema])
async def reorder_steps(
    workspace_id: int,
    seq_id: int,
    payload: SequenceReorderSchema,
    db: AsyncSession = Depends(get_db),
):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    steps = await repo.reorder(seq_id, payload.new_order)
    return steps


@router.put("/{seq_id}/steps", response_model=list[SequenceStepSchema])
async def replace_steps(
    workspace_id: int,
    seq_id: int,
    steps: list[SequenceStepCreateSchema],
    db: AsyncSession = Depends(get_db),
):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    payload = [step.model_dump() for step in steps]
    result = await repo.replace(seq_id, payload)
    return result


# ---------------------------------------------------------------------------
# Runtime control
# ---------------------------------------------------------------------------
@router.post("/{seq_id}/start", response_model=SequenceStateSchema)
async def start_sequence(
    workspace_id: int,
    seq_id: int,
    db: AsyncSession = Depends(get_db),
):
    repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(repo, workspace_id, seq_id)
    try:
        state = await SequenceStateService.get_state(seq_id)
    except SequenceNotFoundError:
        raise HTTPException(status_code=404, detail="Sequence not found")

    if state.status in {"pending", "running", "cancelling"}:
        raise HTTPException(status_code=409, detail="Sequence already running")

    await SequenceCommandService.enqueue_start(seq_id)
    return await SequenceStateService.get_state(seq_id)


@router.post("/{seq_id}/stop", response_model=SequenceStateSchema)
async def stop_sequence(
    workspace_id: int,
    seq_id: int,
    db: AsyncSession = Depends(get_db),
):
    repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(repo, workspace_id, seq_id)
    try:
        state = await SequenceStateService.get_state(seq_id)
    except SequenceNotFoundError:
        raise HTTPException(status_code=404, detail="Sequence not found")

    if state.status not in {"pending", "running", "cancelling"}:
        return state

    await SequenceCommandService.enqueue_stop(seq_id)
    return await SequenceStateService.get_state(seq_id)


@router.get("/{seq_id}/state", response_model=SequenceStateSchema)
async def get_sequence_state(
    workspace_id: int,
    seq_id: int,
    db: AsyncSession = Depends(get_db),
):
    repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(repo, workspace_id, seq_id)
    try:
        return await SequenceStateService.get_state(seq_id)
    except SequenceNotFoundError:
        raise HTTPException(status_code=404, detail="Sequence not found")


# ---------------------------------------------------------------------------
# Import / Export
# ---------------------------------------------------------------------------
@router.get("/{seq_id}/export-file")
async def export_sequence_file(
    workspace_id: int,
    seq_id: int,
    db: AsyncSession = Depends(get_db),
):
    repo = SequenceRepository(db)
    seq = await repo.get(workspace_id, seq_id)
    if not seq:
        raise HTTPException(status_code=404, detail="Sequence not found")

    payload = {
        "name": seq.name,
        "description": seq.description,
        "steps": [
            {
                "order_index": step.order_index,
                "sequence_step_type": step.sequence_step_type.value,
                "unit_id": step.channel.device.unit_id if step.channel and step.channel.device else None,
                "channel_index": step.channel.channel_index if step.channel else None,
                "payload": step.payload,
            }
            for step in seq.steps
        ],
    }

    safe_name = re.sub(r"[^a-zA-Z0-9_-]", "_", seq.name)
    filename = f"{safe_name}_{seq.id}.json"
    return JSONResponse(
        content=payload,
        media_type="application/json",
        headers={"Content-Disposition": f'attachment; filename="{filename}"'},
    )


@router.post("/import-file", response_model=list[SequenceSchema])
async def import_sequences_file(
    workspace_id: int,
    file: UploadFile = File(...),
    db: AsyncSession = Depends(get_db),
):
    raw = await file.read()
    try:
        parsed = json.loads(raw)
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=400, detail=f"Invalid JSON file: {exc}")

    if isinstance(parsed, dict) and "sequences" in parsed:
        seq_payloads = parsed["sequences"]
    elif isinstance(parsed, list):
        seq_payloads = parsed
    else:
        seq_payloads = [parsed]

    try:
        schemas = [SequenceExportSchema.model_validate(item) for item in seq_payloads]
    except ValidationError as exc:  # type: ignore[assignment]
        raise HTTPException(status_code=400, detail=exc.errors())

    repo = SequenceRepository(db)
    imported: List = []
    for schema in schemas:
        steps_data = []
        for step in schema.steps:
            channel_id = await _resolve_channel_id(db, step.unit_id, step.channel_index)
            steps_data.append(
                {
                    "order_index": step.order_index,
                    "sequence_step_type": step.sequence_step_type,
                    "channel_id": channel_id,
                    "payload": step.payload,
                }
            )

        created = await repo.create(
            workspace_id,
            {"name": schema.name, "description": schema.description},
            steps_data,
        )
        imported.append(created)

    return imported


async def _resolve_channel_id(db: AsyncSession, unit_id: str | None, channel_index: int | None) -> int | None:
    if not unit_id or channel_index is None:
        return None

    device_stmt = select(Device).where(Device.unit_id == unit_id)
    device = (await db.execute(device_stmt)).scalar_one_or_none()
    if not device:
        return None

    channel_stmt = select(Channel).where(
        Channel.device_id == device.id,
        Channel.channel_index == channel_index,
    )
    channel = (await db.execute(channel_stmt)).scalar_one_or_none()
    return channel.id if channel else None
