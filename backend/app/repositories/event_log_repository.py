# app/repositories/event_log_repository.py
from typing import List, Optional
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.exc import SQLAlchemyError

from app.models.event_log import EventLog
from app.models.channel import Channel
from app.models.device import Device


class EventLogRepository:
    """Repository for CRUD operations on EventLog."""

    def __init__(self, session: AsyncSession):
        self.session = session

    async def create(self, data: dict) -> EventLog:
        """Insert new event log row into DB."""
        try:
            db_event = EventLog(**data)
            self.session.add(db_event)
            await self.session.commit()
            await self.session.refresh(db_event)
            return db_event
        except SQLAlchemyError as e:
            await self.session.rollback()
            raise RuntimeError(f"DB error creating event log: {e}")

    async def get(self, event_id: int) -> Optional[EventLog]:
        """Get event log by id."""
        result = await self.session.execute(
            select(EventLog).where(EventLog.id == event_id)
        )
        return result.scalar_one_or_none()

    async def list(self, limit: int = 100) -> List[EventLog]:
        """Get latest N events (default 100)."""
        result = await self.session.execute(
            select(EventLog).order_by(EventLog.ts.desc()).limit(limit)
        )
        return result.scalars().all()

    async def delete(self, event_id: int) -> bool:
        """Delete event log entry by id."""
        obj = await self.get(event_id)
        if not obj:
            return False
        await self.session.delete(obj)
        await self.session.commit()
        return True

    async def list_by_channel(self, channel_id: int, limit: int = 100) -> List[EventLog]:
        """Get latest events for specific channel."""
        result = await self.session.execute(
            select(EventLog)
            .where(EventLog.channel_id == channel_id)
            .order_by(EventLog.ts.desc())
            .limit(limit)
        )
        return result.scalars().all()

    async def list_by_unit(self, unit_id: str, limit: int = 100) -> List[EventLog]:
        """Get latest events for all channels of given unit_id."""
        result = await self.session.execute(
            select(EventLog)
            .join(EventLog.channel)
            .join(Channel.device)
            .where(Device.unit_id == unit_id)
            .order_by(EventLog.ts.desc())
            .limit(limit)
        )
        return result.scalars().all()
