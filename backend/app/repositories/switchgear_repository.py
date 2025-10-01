# app/repositories/switchgear_repository.py
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from app.models.switchgear import Switchgear


class SwitchgearRepository:
    """Repository for CRUD operations on Switchgear."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def create(self, data: dict) -> Switchgear:
        try:
            sg = Switchgear(**data)
            self.db.add(sg)
            await self.db.commit()
            await self.db.refresh(sg)
            return sg
        except SQLAlchemyError as e:
            await self.db.rollback()
            raise RuntimeError(f"DB error creating switchgear: {e}")

    async def get(self, sg_id: int) -> Switchgear | None:
        result = await self.db.execute(select(Switchgear).where(Switchgear.id == sg_id))
        return result.scalar_one_or_none()

    async def list(self) -> list[Switchgear]:
        result = await self.db.execute(select(Switchgear))
        return list(result.scalars().all())

    async def update(self, sg_id: int, changes: dict) -> Switchgear | None:
        sg = await self.get(sg_id)
        if not sg:
            return None
        for k, v in changes.items():
            setattr(sg, k, v)
        await self.db.commit()
        await self.db.refresh(sg)
        return sg

    async def delete(self, sg_id: int) -> bool:
        sg = await self.get(sg_id)
        if not sg:
            return False
        await self.db.delete(sg)
        await self.db.commit()
        return True
