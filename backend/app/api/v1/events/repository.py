from __future__ import annotations

from typing import List, Optional

from sqlalchemy import or_, select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.device import Device
from app.models.event import Event


class EventRepository:
    def __init__(self, db: AsyncSession):
        self.db = db

    def _base_query(self):
        return select(Event).options(
            selectinload(Event.channel),
            selectinload(Event.device).selectinload(Device.channels),
        )

    async def list(
        self,
        project_id: Optional[int],
        limit: int = 100,
        include_system: bool = False,
        before_id: Optional[int] = None,
    ) -> List[Event]:
        query = self._base_query()

        if before_id is not None:
            query = query.where(Event.id < before_id)

        if project_id is not None:
            if include_system:
                query = query.where(or_(Event.project_id == project_id, Event.project_id.is_(None)))
            else:
                query = query.where(Event.project_id == project_id)

        query = query.order_by(Event.ts.desc(), Event.id.desc()).limit(limit)
        result = await self.db.execute(query)
        return result.scalars().all()

    async def get(self, project_id: Optional[int], event_id: int) -> Optional[Event]:
        query = self._base_query().where(Event.id == event_id)
        if project_id is not None:
            query = query.where(Event.project_id == project_id)
        result = await self.db.execute(query)
        return result.scalar_one_or_none()

    async def create(self, data: dict) -> Event:
        event = Event(**data)
        self.db.add(event)
        await self.db.flush()
        event_id = event.id
        await self.db.commit()
        refreshed = await self.get_by_id(event_id)
        return refreshed if refreshed is not None else event

    async def delete(self, event_id: int, project_id: Optional[int]) -> bool:
        event = await self.get(project_id, event_id)
        if not event:
            return False
        await self.db.delete(event)
        await self.db.commit()
        return True

    async def get_by_id(self, event_id: int) -> Optional[Event]:
        result = await self.db.execute(self._base_query().where(Event.id == event_id))
        return result.scalar_one_or_none()
