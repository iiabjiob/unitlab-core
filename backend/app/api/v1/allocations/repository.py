from __future__ import annotations

from typing import Optional

from sqlalchemy import delete, select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.allocation import Allocation
from app.models.channel import Channel


class AllocationRepository:
    """Data-access helpers for channel allocations."""

    def __init__(self, db: AsyncSession):
        self.db = db
        self._channel_loader = selectinload(Allocation.channel)

    def _base_query(self):
        return select(Allocation).options(self._channel_loader)

    async def list(self, project_id: int, datapoint_id: Optional[int] = None) -> list[Allocation]:
        stmt = self._base_query().where(Allocation.project_id == project_id)
        if datapoint_id is not None:
            stmt = stmt.where(Allocation.datapoint_id == datapoint_id)
        stmt = stmt.order_by(Allocation.created_at.asc())
        result = await self.db.execute(stmt)
        return list(result.scalars().unique().all())

    async def get(self, project_id: int, allocation_id: int) -> Allocation | None:
        stmt = self._base_query().where(
            Allocation.project_id == project_id,
            Allocation.id == allocation_id,
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def get_by_datapoint(self, project_id: int, datapoint_id: int) -> Allocation | None:
        stmt = self._base_query().where(
            Allocation.project_id == project_id,
            Allocation.datapoint_id == datapoint_id,
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def get_by_channel(self, project_id: int, channel_id: int) -> Allocation | None:
        stmt = self._base_query().where(
            Allocation.project_id == project_id,
            Allocation.channel_id == channel_id,
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def create(self, payload: dict) -> Allocation:
        allocation = Allocation(**payload)
        self.db.add(allocation)
        await self.db.commit()
        await self.db.refresh(allocation)
        return allocation

    async def update(self, allocation: Allocation) -> Allocation:
        await self.db.commit()
        await self.db.refresh(allocation)
        return allocation

    async def delete(self, allocation: Allocation) -> bool:
        await self.db.delete(allocation)
        await self.db.commit()
        return True

    async def delete_by_datapoint(self, project_id: int, datapoint_id: int) -> int:
        stmt = (
            delete(Allocation)
            .where(Allocation.project_id == project_id)
            .where(Allocation.datapoint_id == datapoint_id)
            .execution_options(synchronize_session=False)
        )
        result = await self.db.execute(stmt)
        await self.db.commit()
        return int(result.rowcount or 0)

    async def get_channel(self, channel_id: int) -> Channel | None:
        stmt = select(Channel).where(Channel.id == channel_id)
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()
