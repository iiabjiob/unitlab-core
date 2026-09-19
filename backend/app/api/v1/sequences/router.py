"""REST API for managing sequences, steps, and runtime state."""
from __future__ import annotations

import json
import re
from typing import Annotated, cast

from fastapi import APIRouter, Depends, File, HTTPException, UploadFile
from fastapi.responses import JSONResponse
from pydantic import ValidationError
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.models.channel import Channel
from app.models.device import Device
from app.models.sequence import SequenceStep
from app.api.v1.sequences import SequenceRepository, SequenceStepRepository
from app.api.v1.sequences.errors import ReadOnlySequenceError
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
from app.services.sequence_state_service import SequenceStateService
from app.services.sequence_runner import SequenceNotFoundError

router = APIRouter(prefix="/api/v1/workspaces/{workspace_id}/sequences", tags=["Sequences"])


async def _ensure_sequence_in_workspace(
    repo: SequenceRepository, workspace_id: int, seq_id: int
) -> None:
    if not await repo.ensure(workspace_id, seq_id):
        raise HTTPException(status_code=404, detail="Sequence not found")


async def _validate_nested_sequence_steps(
    repo: SequenceRepository,
    workspace_id: int,
    steps: list[dict[str, object]],
    *,
    current_sequence_id: int | None = None,
) -> None:
    for step in steps:
        step_type = step.get("sequence_step_type") or step.get("type") or step.get("kind")
        if step_type not in {"CALL_SEQUENCE", "REPEAT_SEQUENCE"}:
            continue

        payload = step.get("payload")
        if not isinstance(payload, dict):
            continue
        payload = cast(dict[str, object], payload)
        raw_target = payload.get("target_sequence_id")
        if raw_target is None or str(raw_target).strip() == "":
            # Allow drafts in the editor. Runtime validation still rejects unresolved targets on run.
            continue

        try:
            target_sequence_id = int(str(raw_target))
        except (TypeError, ValueError):
            raise HTTPException(status_code=422, detail=f"{step_type} target_sequence_id must be an integer")

        if target_sequence_id <= 0:
            raise HTTPException(status_code=422, detail=f"{step_type} target_sequence_id must be positive")

        if current_sequence_id is not None and target_sequence_id == current_sequence_id:
            raise HTTPException(status_code=422, detail=f"{step_type} cannot target the current sequence")

        if not await repo.ensure(workspace_id, target_sequence_id):
            raise HTTPException(status_code=422, detail=f"Target sequence {target_sequence_id} is not available in this workspace")


# ---------------------------------------------------------------------------
# CRUD
# ---------------------------------------------------------------------------
@router.get("", response_model=list[SequenceSchema])
async def list_sequences(workspace_id: int, db: Annotated[AsyncSession, Depends(get_db)]):
    repo = SequenceRepository(db)
    return await repo.list(workspace_id)


@router.get("/{seq_id}", response_model=SequenceSchema)
async def get_sequence(workspace_id: int, seq_id: int, db: Annotated[AsyncSession, Depends(get_db)]):
    repo = SequenceRepository(db)
    seq = await repo.get(workspace_id, seq_id)
    if not seq:
        raise HTTPException(status_code=404, detail="Sequence not found")
    return seq


@router.post("", response_model=SequenceSchema)
async def create_sequence(
    workspace_id: int,
    payload: SequenceCreateSchema,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    repo = SequenceRepository(db)
    data = payload.model_dump(exclude={"steps"})
    steps = [step.model_dump() for step in payload.steps]
    await _validate_nested_sequence_steps(repo, workspace_id, steps)
    return await repo.create(workspace_id, data, steps)


@router.patch("/{seq_id}", response_model=SequenceSchema)
async def update_sequence(
    workspace_id: int,
    seq_id: int,
    payload: SequenceUpdateSchema,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    repo = SequenceRepository(db)
    try:
        seq = await repo.update(workspace_id, seq_id, payload.model_dump(exclude_unset=True))
    except ReadOnlySequenceError as exc:
        raise HTTPException(status_code=403, detail=str(exc))
    if not seq:
        raise HTTPException(status_code=404, detail="Sequence not found")
    return seq


@router.delete("/{seq_id}")
async def delete_sequence(workspace_id: int, seq_id: int, db: Annotated[AsyncSession, Depends(get_db)]):
    repo = SequenceRepository(db)
    try:
        deleted = await repo.delete(workspace_id, seq_id)
    except ReadOnlySequenceError as exc:
        raise HTTPException(status_code=403, detail=str(exc))
    if not deleted:
        raise HTTPException(status_code=404, detail="Sequence not found")
    return {"detail": "Sequence deleted"}


# ---------------------------------------------------------------------------
# Steps
# ---------------------------------------------------------------------------
@router.get("/{seq_id}/steps", response_model=list[SequenceStepSchema])
async def list_steps(workspace_id: int, seq_id: int, db: Annotated[AsyncSession, Depends(get_db)]):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    return await repo.get_for_sequence(seq_id)


@router.post("/{seq_id}/steps", response_model=SequenceStepSchema)
async def create_step(
    workspace_id: int,
    seq_id: int,
    payload: SequenceStepCreateSchema,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    await _validate_nested_sequence_steps(seq_repo, workspace_id, [payload.model_dump(exclude_unset=True)], current_sequence_id=seq_id)
    try:
        step = await repo.create(seq_id, payload.model_dump(exclude_unset=True))
    except ReadOnlySequenceError as exc:
        raise HTTPException(status_code=403, detail=str(exc))
    return step


@router.patch("/{seq_id}/steps/{step_id}", response_model=SequenceStepSchema)
async def update_step(
    workspace_id: int,
    seq_id: int,
    step_id: int,
    payload: SequenceStepUpdateSchema,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    existing = await repo.get(step_id)
    update_payload = payload.model_dump(exclude_unset=True)
    if existing:
        if "sequence_step_type" not in update_payload:
            update_payload["sequence_step_type"] = existing.sequence_step_type.value
        if "payload" not in update_payload and existing.payload is not None:
            update_payload["payload"] = existing.payload
        elif "payload" in update_payload and isinstance(existing.payload, dict):
            update_payload["payload"] = {**existing.payload, **(update_payload.get("payload") or {})}
    await _validate_nested_sequence_steps(seq_repo, workspace_id, [update_payload], current_sequence_id=seq_id)
    try:
        step = await repo.update(step_id, payload.model_dump(exclude_unset=True))
    except ReadOnlySequenceError as exc:
        raise HTTPException(status_code=403, detail=str(exc))
    if not step:
        raise HTTPException(status_code=404, detail="Step not found")
    return step


@router.delete("/{seq_id}/steps/{step_id}")
async def delete_step(workspace_id: int, seq_id: int, step_id: int, db: Annotated[AsyncSession, Depends(get_db)]):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    try:
        deleted = await repo.delete(step_id)
    except ReadOnlySequenceError as exc:
        raise HTTPException(status_code=403, detail=str(exc))
    if not deleted:
        raise HTTPException(status_code=404, detail="Step not found")

    _ = await repo.normalize(seq_id)
    return {"detail": "Step deleted"}


@router.post("/{seq_id}/steps/reorder", response_model=list[SequenceStepSchema])
async def reorder_steps(
    workspace_id: int,
    seq_id: int,
    payload: SequenceReorderSchema,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    try:
        steps = await repo.reorder(seq_id, payload.new_order)
    except ReadOnlySequenceError as exc:
        raise HTTPException(status_code=403, detail=str(exc))
    return steps


@router.put("/{seq_id}/steps", response_model=list[SequenceStepSchema])
async def replace_steps(
    workspace_id: int,
    seq_id: int,
    steps: list[SequenceStepCreateSchema],
    db: Annotated[AsyncSession, Depends(get_db)],
):
    seq_repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(seq_repo, workspace_id, seq_id)
    repo = SequenceStepRepository(db)
    payload = [step.model_dump() for step in steps]
    await _validate_nested_sequence_steps(seq_repo, workspace_id, payload, current_sequence_id=seq_id)
    try:
        result = await repo.replace(seq_id, payload)
    except ReadOnlySequenceError as exc:
        raise HTTPException(status_code=403, detail=str(exc))
    return result


# ---------------------------------------------------------------------------
# Runtime control
# ---------------------------------------------------------------------------
@router.post("/{seq_id}/start", response_model=SequenceStateSchema)
async def start_sequence(
    workspace_id: int,
    seq_id: int,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(repo, workspace_id, seq_id)
    _ = await SequenceCommandService.enqueue_start(seq_id, workspace_id=workspace_id)
    try:
        return await SequenceStateService.get_state(seq_id)
    except SequenceNotFoundError:
        raise HTTPException(status_code=404, detail="Sequence not found")


@router.post("/{seq_id}/stop", response_model=SequenceStateSchema)
async def stop_sequence(
    workspace_id: int,
    seq_id: int,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    repo = SequenceRepository(db)
    await _ensure_sequence_in_workspace(repo, workspace_id, seq_id)
    _ = await SequenceCommandService.enqueue_stop(seq_id)
    try:
        return await SequenceStateService.get_state(seq_id)
    except SequenceNotFoundError:
        raise HTTPException(status_code=404, detail="Sequence not found")


@router.get("/{seq_id}/state", response_model=SequenceStateSchema)
async def get_sequence_state(
    workspace_id: int,
    seq_id: int,
    db: Annotated[AsyncSession, Depends(get_db)],
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
    db: Annotated[AsyncSession, Depends(get_db)],
):
    repo = SequenceRepository(db)
    seq = await repo.get(workspace_id, seq_id)
    if not seq:
        raise HTTPException(status_code=404, detail="Sequence not found")

    export_steps: list[dict[str, object]] = []
    for raw_step in cast(list[object], cast(object, seq.steps)):
        step = cast(SequenceStep, raw_step)
        channel = cast(Channel | None, cast(object, step.channel))
        device = cast(Device | None, channel.device) if channel else None
        export_steps.append({
            "order_index": step.order_index,
            "sequence_step_type": step.sequence_step_type.value,
            "unit_id": device.unit_id if device else None,
            "channel_index": channel.channel_index if channel else None,
            "payload": step.payload,
        })
    payload = {
        "name": seq.name,
        "description": seq.description,
        "steps": export_steps,
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
    file: Annotated[UploadFile, File(...)],
    db: Annotated[AsyncSession, Depends(get_db)],
):
    raw = await file.read()
    try:
        parsed = cast(object, json.loads(raw))
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=400, detail=f"Invalid JSON file: {exc}")

    seq_payloads: list[object]
    if isinstance(parsed, dict) and "sequences" in parsed:
        raw_sequences = cast(dict[str, object], parsed).get("sequences")
        seq_payloads = cast(list[object], raw_sequences) if isinstance(raw_sequences, list) else []
    elif isinstance(parsed, list):
        seq_payloads = cast(list[object], parsed)
    else:
        seq_payloads = [parsed]

    try:
        schemas = [SequenceExportSchema.model_validate(item) for item in seq_payloads]
    except ValidationError as exc:  # type: ignore[assignment]
        raise HTTPException(status_code=400, detail=exc.errors())

    repo = SequenceRepository(db)
    imported: list[SequenceSchema] = []
    for schema in schemas:
        steps_data: list[dict[str, object]] = []
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

        await _validate_nested_sequence_steps(repo, workspace_id, steps_data)

        created = await repo.create(
            workspace_id,
            {"name": schema.name, "description": schema.description},
            steps_data,
        )
        imported.append(SequenceSchema.model_validate(created))

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
