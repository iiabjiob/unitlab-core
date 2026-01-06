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

router = APIRouter(prefix="/api/v1/projects/{project_id}/switchgears", tags=["Switchgears"])


def get_repository(db: AsyncSession = Depends(get_db)) -> SwitchgearRepository:
    return SwitchgearRepository(db)


@router.get("", response_model=list[SwitchgearSchema])
async def list_switchgears(
    project_id: int,
    repo: SwitchgearRepository = Depends(get_repository),
):
    return await repo.list(project_id)


@router.get("/{switchgear_id}", response_model=SwitchgearSchema)
async def get_switchgear(
    project_id: int,
    switchgear_id: int,
    repo: SwitchgearRepository = Depends(get_repository),
):
    switchgear = await repo.get(project_id, switchgear_id)
    if not switchgear:
        raise HTTPException(status_code=404, detail="Switchgear not found")
    return switchgear


@router.post("", response_model=SwitchgearSchema)
async def create_switchgear(
    project_id: int,
    payload: SwitchgearCreateSchema,
    repo: SwitchgearRepository = Depends(get_repository),
):
    return await repo.create(project_id, payload.model_dump())


@router.patch("/{switchgear_id}", response_model=SwitchgearSchema)
async def update_switchgear(
    project_id: int,
    switchgear_id: int,
    payload: SwitchgearUpdateSchema,
    repo: SwitchgearRepository = Depends(get_repository),
):
    updated = await repo.update(
        project_id,
        switchgear_id,
        payload.model_dump(exclude_unset=True),
    )
    if not updated:
        raise HTTPException(status_code=404, detail="Switchgear not found")
    return updated


@router.delete("/{switchgear_id}")
async def delete_switchgear(
    project_id: int,
    switchgear_id: int,
    repo: SwitchgearRepository = Depends(get_repository),
):
    deleted = await repo.delete(project_id, switchgear_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Switchgear not found")
    return {"detail": "Switchgear deleted"}
