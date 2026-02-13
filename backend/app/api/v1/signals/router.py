from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.signals import SignalsRepository
from app.infrastructure.db.database import get_db
from app.schemas.signal_schema import SignalCreateSchema, SignalSchema, SignalUpdateSchema

router = APIRouter(prefix="/api/v1", tags=["Signals"])


def get_repo(db: AsyncSession = Depends(get_db)) -> SignalsRepository:
    return SignalsRepository(db)


@router.get("/workspaces/{workspace_id}/signals", response_model=list[SignalSchema])
async def list_signals(workspace_id: int, repo: SignalsRepository = Depends(get_repo)):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")
    return await repo.list(workspace_id)


@router.post("/workspaces/{workspace_id}/signals", response_model=SignalSchema)
async def create_signal(
    workspace_id: int,
    payload: SignalCreateSchema,
    repo: SignalsRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")
    try:
        return await repo.create(workspace_id, payload.model_dump())
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc))


@router.get("/signals/{signal_id}", response_model=SignalSchema)
async def get_signal(signal_id: int, repo: SignalsRepository = Depends(get_repo)):
    signal = await repo.get(signal_id)
    if not signal or signal.deleted_at is not None:
        raise HTTPException(status_code=404, detail="Signal not found")
    return signal


@router.put("/signals/{signal_id}", response_model=SignalSchema)
async def update_signal(
    signal_id: int,
    payload: SignalUpdateSchema,
    repo: SignalsRepository = Depends(get_repo),
):
    try:
        signal = await repo.update(signal_id, payload.model_dump(exclude_unset=True))
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc))
    if not signal or signal.deleted_at is not None:
        raise HTTPException(status_code=404, detail="Signal not found")
    return signal


@router.delete("/signals/{signal_id}")
async def delete_signal(signal_id: int, repo: SignalsRepository = Depends(get_repo)):
    deleted = await repo.delete(signal_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Signal not found")
    return {"detail": "Signal deleted"}
