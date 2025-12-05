from __future__ import annotations

from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.switchgear import Switchgear


class SwitchgearRepository:
    """CRUD helpers for switchgears scoped to the v1 API."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def list(self) -> list[Switchgear]:
        result = await self.db.execute(select(Switchgear))
        return list(result.scalars().all())

    async def get(self, switchgear_id: int) -> Switchgear | None:
        result = await self.db.execute(select(Switchgear).where(Switchgear.id == switchgear_id))
        return result.scalar_one_or_none()

    async def create(self, data: dict) -> Switchgear:
        try:
            switchgear = Switchgear(**data)
            self.db.add(switchgear)
            await self.db.commit()
            await self.db.refresh(switchgear)
            return switchgear
        except SQLAlchemyError as exc:  # pragma: no cover
            await self.db.rollback()
            raise RuntimeError(f"DB error creating switchgear: {exc}") from exc

    async def update(self, switchgear_id: int, changes: dict) -> Switchgear | None:
        switchgear = await self.get(switchgear_id)
        if not switchgear:
            return None
        for key, value in changes.items():
            setattr(switchgear, key, value)
        await self.db.commit()
        await self.db.refresh(switchgear)
        return switchgear

    async def delete(self, switchgear_id: int) -> bool:
        switchgear = await self.get(switchgear_id)
        if not switchgear:
            return False
        await self.db.delete(switchgear)
        await self.db.commit()
        return True
