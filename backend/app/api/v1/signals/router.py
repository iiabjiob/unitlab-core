from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.models.workspace import Workspace
from app.schemas.signal_schema import SignalCreateSchema, SignalSchema, SignalUpdateSchema
from app.services.signal_service import (
    SignalKeyConflictError,
    SignalNotFoundError,
    SignalService,
)

router = APIRouter(prefix="/api/v1", tags=["Signals"])


def _signal_service(db: AsyncSession) -> SignalService:
    return SignalService(db)


async def _ensure_workspace(db: AsyncSession, workspace_id: int) -> None:
    stmt = select(Workspace.id).where(Workspace.id == workspace_id)
    result = await db.execute(stmt)
    if result.scalar_one_or_none() is None:
        raise HTTPException(status_code=404, detail="Workspace not found")


@router.get("/workspaces/{workspace_id}/signals", response_model=list[SignalSchema])
async def list_signals(workspace_id: int, db: AsyncSession = Depends(get_db)):
    await _ensure_workspace(db, workspace_id)
    service = _signal_service(db)
    return await service.list_signals(workspace_id)


@router.post("/workspaces/{workspace_id}/signals", response_model=SignalSchema)
async def create_signal(
    workspace_id: int,
    payload: SignalCreateSchema,
    db: AsyncSession = Depends(get_db),
):
    await _ensure_workspace(db, workspace_id)
    service = _signal_service(db)
    try:
        return await service.create_signal(workspace_id, payload)
    except SignalKeyConflictError:
        raise HTTPException(status_code=409, detail="Signal key already exists in workspace")


@router.get("/signals/{signal_id}", response_model=SignalSchema)
async def get_signal(signal_id: int, db: AsyncSession = Depends(get_db)):
    service = _signal_service(db)
    try:
        return await service.get_signal(signal_id)
    except SignalNotFoundError:
        raise HTTPException(status_code=404, detail="Signal not found")


@router.put("/signals/{signal_id}", response_model=SignalSchema)
async def update_signal(
    signal_id: int,
    payload: SignalUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    service = _signal_service(db)
    try:
        return await service.update_signal(signal_id, payload)
    except SignalNotFoundError:
        raise HTTPException(status_code=404, detail="Signal not found")


@router.delete("/signals/{signal_id}")
async def delete_signal(signal_id: int, db: AsyncSession = Depends(get_db)):
    service = _signal_service(db)
    try:
        await service.delete_signal(signal_id)
    except SignalNotFoundError:
        raise HTTPException(status_code=404, detail="Signal not found")
    return {"detail": "Signal deleted"}
