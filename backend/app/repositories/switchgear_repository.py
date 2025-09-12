# app/repositories/switchgear_repository.py
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from app.models.switchgear import Switchgear

async def create_switchgear(db: AsyncSession, data: dict) -> Switchgear:
    try:
        sg = Switchgear(**data)
        db.add(sg)
        await db.commit()
        await db.refresh(sg)
        return sg
    except SQLAlchemyError as e:
        await db.rollback()
        raise RuntimeError(f"DB error creating switchgear: {e}")

async def get_switchgear(db: AsyncSession, sg_id: int) -> Switchgear | None:
    result = await db.execute(select(Switchgear).where(Switchgear.id == sg_id))
    return result.scalar_one_or_none()

async def get_all_switchgear(db: AsyncSession) -> list[Switchgear]:
    result = await db.execute(select(Switchgear))
    return list(result.scalars().all())

async def update_switchgear(db: AsyncSession, sg_id: int, changes: dict) -> Switchgear | None:
    sg = await get_switchgear(db, sg_id)
    if not sg:
        return None
    for k, v in changes.items():
        setattr(sg, k, v)
    await db.commit()
    await db.refresh(sg)
    return sg

async def delete_switchgear(db: AsyncSession, sg_id: int) -> bool:
    sg = await get_switchgear(db, sg_id)
    if not sg:
        return False
    await db.delete(sg)
    await db.commit()
    return True
