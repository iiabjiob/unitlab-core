from __future__ import annotations

from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.switchgear import Switchgear, SwitchgearChannelBinding


class SwitchgearRepository:
    """CRUD helpers for switchgears scoped to the v1 API."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def list(self) -> list[Switchgear]:
        result = await self.db.execute(select(Switchgear).options(selectinload(Switchgear.bindings)))
        return list(result.scalars().all())

    async def get(self, switchgear_id: int) -> Switchgear | None:
        stmt = select(Switchgear).options(selectinload(Switchgear.bindings)).where(
            Switchgear.id == switchgear_id
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def create(self, data: dict) -> Switchgear:
        try:
            bindings_data = data.pop("bindings", [])
            switchgear = Switchgear(**data)
            for binding in bindings_data:
                switchgear.bindings.append(SwitchgearChannelBinding(**binding))
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
        bindings_data = changes.pop("bindings", None)
        for key, value in changes.items():
            setattr(switchgear, key, value)

        if bindings_data is not None:
            switchgear.bindings.clear()
            for binding in bindings_data:
                switchgear.bindings.append(SwitchgearChannelBinding(**binding))

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
