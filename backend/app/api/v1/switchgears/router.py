from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.switchgears import SwitchgearRepository
from app.infrastructure.db.database import get_db
from app.schemas.switchgear_schema import (
    SwitchgearCreateSchema,
    SwitchgearSchema,
    SwitchgearUpdateSchema,
)

router = APIRouter(prefix="/api/v1/switchgears", tags=["Switchgears"])


def get_repository(db: AsyncSession = Depends(get_db)) -> SwitchgearRepository:
    return SwitchgearRepository(db)


@router.get("", response_model=list[SwitchgearSchema])
async def list_switchgears(repo: SwitchgearRepository = Depends(get_repository)):
    return await repo.list()


@router.get("/{switchgear_id}", response_model=SwitchgearSchema)
async def get_switchgear(
    switchgear_id: int,
    repo: SwitchgearRepository = Depends(get_repository),
):
    switchgear = await repo.get(switchgear_id)
    if not switchgear:
        raise HTTPException(status_code=404, detail="Switchgear not found")
    return switchgear


@router.post("", response_model=SwitchgearSchema)
async def create_switchgear(
    payload: SwitchgearCreateSchema,
    repo: SwitchgearRepository = Depends(get_repository),
):
    return await repo.create(payload.model_dump())


@router.patch("/{switchgear_id}", response_model=SwitchgearSchema)
async def update_switchgear(
    switchgear_id: int,
    payload: SwitchgearUpdateSchema,
    repo: SwitchgearRepository = Depends(get_repository),
):
    updated = await repo.update(switchgear_id, payload.model_dump(exclude_unset=True))
    if not updated:
        raise HTTPException(status_code=404, detail="Switchgear not found")
    return updated


@router.delete("/{switchgear_id}")
async def delete_switchgear(
    switchgear_id: int,
    repo: SwitchgearRepository = Depends(get_repository),
):
    deleted = await repo.delete(switchgear_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Switchgear not found")
    return {"detail": "Switchgear deleted"}
