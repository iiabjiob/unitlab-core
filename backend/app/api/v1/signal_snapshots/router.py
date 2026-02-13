from __future__ import annotations

import hashlib
import json
from typing import Any

from fastapi import APIRouter, Depends, File, Form, HTTPException, Query, UploadFile
from pydantic import ValidationError
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.signal_snapshots import SignalSnapshotsRepository
from app.api.v1.signals import SignalsRepository
from app.infrastructure.db.database import get_db
from app.models.signal_snapshot import SignalSnapshotStatus
from app.schemas.signal_snapshot_schema import (
    SignalImportMetaSchema,
    SignalSnapshotAllocationSchema,
    SignalSnapshotAllocationUpdateSchema,
    SignalSnapshotSchema,
    SignalSnapshotSummarySchema,
)
from app.services.signal_sheet_import_service import SignalSheetImportService

router = APIRouter(prefix="/api/v1", tags=["Signal Snapshots"])


def get_snapshot_repo(db: AsyncSession = Depends(get_db)) -> SignalSnapshotsRepository:
    return SignalSnapshotsRepository(db)


def get_signals_repo(db: AsyncSession = Depends(get_db)) -> SignalsRepository:
    return SignalsRepository(db)


@router.get("/workspaces/{workspace_id}/signal-snapshots", response_model=list[SignalSnapshotSummarySchema])
async def list_signal_snapshots(
    workspace_id: int,
    limit: int = Query(default=200, ge=1, le=1000),
    offset: int = Query(default=0, ge=0),
    repo: SignalSnapshotsRepository = Depends(get_snapshot_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")
    return await repo.list(workspace_id, limit=limit, offset=offset)


@router.post("/workspaces/{workspace_id}/signal-snapshots/import", response_model=SignalSnapshotSchema)
async def import_signal_snapshot(
    workspace_id: int,
    file: UploadFile = File(...),
    metadata: str | None = Form(default=None),
    repo: SignalSnapshotsRepository = Depends(get_snapshot_repo),
    signals_repo: SignalsRepository = Depends(get_signals_repo),
    db: AsyncSession = Depends(get_db),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    parsed_meta = _parse_metadata(metadata)
    raw = await file.read()
    if not raw:
        raise HTTPException(status_code=400, detail="Uploaded file is empty")

    try:
        payload = SignalSheetImportService.parse_workbook(
            raw,
            filename=file.filename,
            metadata=parsed_meta,
        )
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=400, detail=f"Unable to parse workbook: {exc}")

    if payload.signals:
        await signals_repo.upsert_imported(workspace_id, payload.signals)
        await db.commit()

    source_hash = hashlib.sha256(raw).hexdigest()
    snapshot = await repo.create(
        workspace_id=workspace_id,
        source_filename=file.filename,
        source_hash=source_hash,
        rows_count=payload.rows_count,
        schema_version=2,
        data=payload.data,
        import_meta=parsed_meta.model_dump() if parsed_meta else None,
    )

    return snapshot


@router.get("/signal-snapshots/{snapshot_id}", response_model=SignalSnapshotSchema)
async def get_signal_snapshot(snapshot_id: int, repo: SignalSnapshotsRepository = Depends(get_snapshot_repo)):
    snapshot = await repo.get(snapshot_id)
    if not snapshot:
        raise HTTPException(status_code=404, detail="Signal snapshot not found")
    return snapshot


@router.delete("/signal-snapshots/{snapshot_id}")
async def delete_signal_snapshot(snapshot_id: int, repo: SignalSnapshotsRepository = Depends(get_snapshot_repo)):
    deleted = await repo.delete(snapshot_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Signal snapshot not found")
    return {"detail": "Signal snapshot deleted"}


@router.post("/signal-snapshots/{snapshot_id}/lock", response_model=SignalSnapshotSchema)
async def lock_signal_snapshot(snapshot_id: int, repo: SignalSnapshotsRepository = Depends(get_snapshot_repo)):
    snapshot = await repo.lock(snapshot_id)
    if not snapshot:
        raise HTTPException(status_code=404, detail="Signal snapshot not found")
    return snapshot


@router.get("/signal-snapshots/{snapshot_id}/allocation", response_model=SignalSnapshotAllocationSchema)
async def get_signal_snapshot_allocation(
    snapshot_id: int,
    repo: SignalSnapshotsRepository = Depends(get_snapshot_repo),
):
    allocation = await repo.get_allocation(snapshot_id)
    if not allocation:
        raise HTTPException(status_code=404, detail="Allocation not found")
    return allocation


@router.put("/signal-snapshots/{snapshot_id}/allocation", response_model=SignalSnapshotAllocationSchema)
async def update_signal_snapshot_allocation(
    snapshot_id: int,
    payload: SignalSnapshotAllocationUpdateSchema,
    repo: SignalSnapshotsRepository = Depends(get_snapshot_repo),
):
    snapshot = await repo.get(snapshot_id)
    if not snapshot:
        raise HTTPException(status_code=404, detail="Signal snapshot not found")
    if snapshot.status == SignalSnapshotStatus.LOCKED:
        raise HTTPException(status_code=409, detail="Locked snapshot cannot be modified")

    mapping_dump = [item.model_dump() for item in payload.mapping]

    channel_ids = set()
    for item in mapping_dump:
        channel_raw = item.get("channel_id")
        if channel_raw is None:
            continue
        try:
            channel_ids.add(int(channel_raw))
        except (TypeError, ValueError):
            raise HTTPException(status_code=400, detail=f"Invalid channel_id: {channel_raw}")

    if channel_ids and not await repo.channels_exist(channel_ids):
        raise HTTPException(status_code=400, detail="Allocation references unknown channels")

    allocation = await repo.update_allocation(snapshot_id, mapping_dump)
    if not allocation:
        raise HTTPException(status_code=404, detail="Allocation not found")
    return allocation


def _parse_metadata(raw_metadata: str | None) -> SignalImportMetaSchema | None:
    if raw_metadata is None:
        return None

    try:
        payload: dict[str, Any] = json.loads(raw_metadata)
    except json.JSONDecodeError as exc:
        raise HTTPException(status_code=400, detail=f"Invalid metadata JSON: {exc}")

    try:
        return SignalImportMetaSchema.model_validate(payload)
    except ValidationError as exc:
        raise HTTPException(status_code=400, detail=exc.errors())
