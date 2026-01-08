from __future__ import annotations

from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.datapoint import Datapoint
from app.models.allocation import Allocation


class DatapointRepository:
    """Data-access helpers for datapoints."""

    def __init__(self, db: AsyncSession):
        self.db = db
        self._allocation_loader = selectinload(Datapoint.allocations).selectinload(Allocation.channel)

    def _base_query(self):
        return select(Datapoint).options(self._allocation_loader).order_by(Datapoint.id.asc())

    async def list(self, project_id: int) -> list[Datapoint]:
        stmt = self._base_query().where(Datapoint.project_id == project_id)
        result = await self.db.execute(stmt)
        return list(result.scalars().unique().all())

    async def list_paginated(self, project_id: int, limit: int, offset: int) -> tuple[list[Datapoint], int]:
        stmt = (
            self._base_query()
            .where(Datapoint.project_id == project_id)
            .limit(limit)
            .offset(offset)
        )
        result = await self.db.execute(stmt)
        items = list(result.scalars().unique().all())

        total_stmt = select(func.count(Datapoint.id)).where(Datapoint.project_id == project_id)
        total_result = await self.db.execute(total_stmt)
        total = int(total_result.scalar_one() or 0)

        return items, total

    async def get(self, project_id: int, datapoint_id: int) -> Datapoint | None:
        stmt = self._base_query().where(
            Datapoint.project_id == project_id,
            Datapoint.id == datapoint_id,
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def create(self, project_id: int, data: dict) -> Datapoint:
        datapoint = Datapoint(project_id=project_id, **data)
        self.db.add(datapoint)
        await self.db.commit()
        await self.db.refresh(datapoint)
        return datapoint

    async def update(self, project_id: int, datapoint_id: int, changes: dict) -> Datapoint | None:
        datapoint = await self.get(project_id, datapoint_id)
        if not datapoint:
            return None
        for key, value in changes.items():
            setattr(datapoint, key, value)
        await self.db.commit()
        await self.db.refresh(datapoint)
        return datapoint

    async def delete(self, project_id: int, datapoint_id: int) -> bool:
        datapoint = await self.get(project_id, datapoint_id)
        if not datapoint:
            return False
        await self.db.delete(datapoint)
        await self.db.commit()
        return True
