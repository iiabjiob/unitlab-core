from __future__ import annotations

from fastapi import APIRouter, Depends, File, HTTPException, UploadFile
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.models.workspace import Workspace
from app.schemas.allocation_schema import AllocationSchema, AllocationUpdateSchema
from app.schemas.signal_snapshot_schema import (
    SignalSnapshotDetailSchema,
    SignalSnapshotSummarySchema,
)
from app.services.allocation_service import AllocationService
from app.services.signal_snapshot_service import (
    InvalidCSVError,
    SignalSnapshotNotFoundError,
    SignalSnapshotService,
    SnapshotLockedError,
)

router = APIRouter(prefix="/api/v1", tags=["SignalSnapshots"])


async def _ensure_workspace(db: AsyncSession, workspace_id: int) -> None:
    stmt = select(Workspace.id).where(Workspace.id == workspace_id)
    result = await db.execute(stmt)
    if result.scalar_one_or_none() is None:
        raise HTTPException(status_code=404, detail="Workspace not found")


@router.post(
    "/workspaces/{workspace_id}/signal-snapshots/import",
    response_model=SignalSnapshotDetailSchema,
)
async def import_signal_snapshot(
    workspace_id: int,
    file: UploadFile = File(...),
    db: AsyncSession = Depends(get_db),
):
    await _ensure_workspace(db, workspace_id)
    service = SignalSnapshotService(db)
    content = await file.read()
    try:
        snapshot = await service.import_csv(workspace_id, content, file.filename)
    except InvalidCSVError as exc:
        raise HTTPException(status_code=400, detail=str(exc))
    return snapshot


@router.get(
    "/workspaces/{workspace_id}/signal-snapshots",
    response_model=list[SignalSnapshotSummarySchema],
)
async def list_signal_snapshots(workspace_id: int, db: AsyncSession = Depends(get_db)):
    await _ensure_workspace(db, workspace_id)
    service = SignalSnapshotService(db)
    return await service.list_snapshots(workspace_id)


@router.get("/signal-snapshots/{snapshot_id}", response_model=SignalSnapshotDetailSchema)
async def get_signal_snapshot(snapshot_id: int, db: AsyncSession = Depends(get_db)):
    service = SignalSnapshotService(db)
    try:
        snapshot = await service.get_snapshot(snapshot_id)
    except SignalSnapshotNotFoundError:
        raise HTTPException(status_code=404, detail="Signal snapshot not found")
    return snapshot


@router.delete("/signal-snapshots/{snapshot_id}")
async def delete_signal_snapshot(snapshot_id: int, db: AsyncSession = Depends(get_db)):
    service = SignalSnapshotService(db)
    try:
        await service.delete_snapshot(snapshot_id)
    except SignalSnapshotNotFoundError:
        raise HTTPException(status_code=404, detail="Signal snapshot not found")
    except SnapshotLockedError:
        raise HTTPException(status_code=409, detail="Locked snapshots cannot be deleted")
    return {"detail": "Signal snapshot deleted"}


@router.get(
    "/signal-snapshots/{snapshot_id}/allocation",
    response_model=AllocationSchema,
)
async def get_allocation(snapshot_id: int, db: AsyncSession = Depends(get_db)):
    service = AllocationService(db)
    try:
        allocation = await service.get_allocation(snapshot_id)
    except SignalSnapshotNotFoundError:
        raise HTTPException(status_code=404, detail="Signal snapshot not found")
    return allocation


@router.put(
    "/signal-snapshots/{snapshot_id}/allocation",
    response_model=AllocationSchema,
)
async def update_allocation(
    snapshot_id: int,
    payload: AllocationUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    service = AllocationService(db)
    try:
        allocation = await service.set_allocation(
            snapshot_id, [item.model_dump() for item in payload.mapping]
        )
    except SignalSnapshotNotFoundError:
        raise HTTPException(status_code=404, detail="Signal snapshot not found")
    except SnapshotLockedError:
        raise HTTPException(status_code=409, detail="Allocation cannot be updated once locked")
    return allocation
