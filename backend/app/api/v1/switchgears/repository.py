from __future__ import annotations

from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.switchgear import Switchgear, SwitchgearChannelBinding


class SwitchgearRepository:
    """CRUD helpers for project-scoped switchgears."""

    def __init__(self, db: AsyncSession):
        self.db = db
        self._binding_loader = selectinload(Switchgear.bindings).selectinload(
            SwitchgearChannelBinding.channel
        )

    def _base_query(self):
        return select(Switchgear).options(self._binding_loader)

    async def list(self, project_id: int) -> list[Switchgear]:
        stmt = (
            self._base_query()
            .where(Switchgear.project_id == project_id)
            .order_by(Switchgear.name.asc())
        )
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def get(self, project_id: int, switchgear_id: int) -> Switchgear | None:
        stmt = self._base_query().where(
            Switchgear.id == switchgear_id,
            Switchgear.project_id == project_id,
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def create(self, project_id: int, data: dict) -> Switchgear:
        try:
            bindings_data = data.pop("bindings", [])
            switchgear = Switchgear(project_id=project_id, **data)
            for binding in bindings_data:
                switchgear.bindings.append(SwitchgearChannelBinding(**binding))
            self.db.add(switchgear)
            await self.db.commit()
            await self.db.refresh(switchgear)
            return switchgear
        except SQLAlchemyError as exc:  # pragma: no cover
            await self.db.rollback()
            raise RuntimeError(f"DB error creating switchgear: {exc}") from exc

    async def update(self, project_id: int, switchgear_id: int, changes: dict) -> Switchgear | None:
        switchgear = await self.get(project_id, switchgear_id)
        if not switchgear:
            return None
        bindings_data = changes.pop("bindings", None)
        for key, value in changes.items():
            setattr(switchgear, key, value)

        if bindings_data is not None:
            switchgear.bindings.clear()
            await self.db.flush()
            for binding in bindings_data:
                switchgear.bindings.append(SwitchgearChannelBinding(**binding))

        await self.db.commit()
        await self.db.refresh(switchgear)
        return switchgear

    async def delete(self, project_id: int, switchgear_id: int) -> bool:
        switchgear = await self.get(project_id, switchgear_id)
        if not switchgear:
            return False
        await self.db.delete(switchgear)
        await self.db.commit()
        return True
