# app/api/switchgear_router.py
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.repositories.switchgear_repository import SwitchgearRepository
from app.schemas.switchgear_schema import (
    SwitchgearSchema,
    SwitchgearCreateSchema,
    SwitchgearUpdateSchema,
)

router = APIRouter(prefix="/api/switchgears", tags=["Switchgears"])


@router.get("", response_model=list[SwitchgearSchema])
async def list_switchgears(db: AsyncSession = Depends(get_db)):
    repo = SwitchgearRepository(db)
    return await repo.get_all()


@router.get("/{sg_id}", response_model=SwitchgearSchema)
async def get_switchgear(sg_id: int, db: AsyncSession = Depends(get_db)):
    repo = SwitchgearRepository(db)
    sg = await repo.get(sg_id)
    if not sg:
        raise HTTPException(status_code=404, detail="Switchgear not found")
    return sg


@router.post("", response_model=SwitchgearSchema)
async def create_switchgear(
    data: SwitchgearCreateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = SwitchgearRepository(db)
    return await repo.create(data.model_dump())


@router.patch("/{sg_id}", response_model=SwitchgearSchema)
async def update_switchgear(
    sg_id: int,
    data: SwitchgearUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = SwitchgearRepository(db)
    sg = await repo.update(sg_id, data.model_dump(exclude_unset=True))
    if not sg:
        raise HTTPException(status_code=404, detail="Switchgear not found")
    return sg


@router.delete("/{sg_id}")
async def delete_switchgear(sg_id: int, db: AsyncSession = Depends(get_db)):
    repo = SwitchgearRepository(db)
    ok = await repo.delete(sg_id)
    if not ok:
        raise HTTPException(status_code=404, detail="Switchgear not found")
    return {"detail": "Switchgear deleted"}
