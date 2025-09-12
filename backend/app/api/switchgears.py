# app/api/switchgear_router.py
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession
from app.infrastructure.db.database import get_db
from app.repositories.switchgear_repository import (
    create_switchgear, get_all_switchgear, get_switchgear,
    update_switchgear, delete_switchgear
)
from app.schemas.switchgear_schema import (
    SwitchgearSchema, SwitchgearCreateSchema, SwitchgearUpdateSchema
)

router = APIRouter(prefix="/api/switchgears", tags=["Switchgears"])

@router.get("", response_model=list[SwitchgearSchema])
async def list_switchgear(db: AsyncSession = Depends(get_db)):
    return await get_all_switchgear(db)

@router.get("/{sg_id}", response_model=SwitchgearSchema)
async def get_one(sg_id: int, db: AsyncSession = Depends(get_db)):
    sg = await get_switchgear(db, sg_id)
    if not sg:
        raise HTTPException(404, "Switchgear not found")
    return sg

@router.post("", response_model=SwitchgearSchema)
async def create(data: SwitchgearCreateSchema, db: AsyncSession = Depends(get_db)):
    return await create_switchgear(db, data.model_dump())

@router.patch("/{sg_id}", response_model=SwitchgearSchema)
async def update(sg_id: int, data: SwitchgearUpdateSchema, db: AsyncSession = Depends(get_db)):
    sg = await update_switchgear(db, sg_id, data.model_dump(exclude_unset=True))
    if not sg:
        raise HTTPException(404, "Switchgear not found")
    return sg

@router.delete("/{sg_id}")
async def delete(sg_id: int, db: AsyncSession = Depends(get_db)):
    ok = await delete_switchgear(db, sg_id)
    if not ok:
        raise HTTPException(404, "Switchgear not found")
    return {"detail": "Switchgear deleted"}
