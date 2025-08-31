from typing import List, Optional
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.event_log import EventLog


class EventLogRepository:
    @staticmethod
    async def create(session: AsyncSession, data: dict) -> EventLog:
        """
        Insert new event log row into DB.
        """
        db_event = EventLog(**data)
        session.add(db_event)
        await session.commit()
        await session.refresh(db_event)
        return db_event

    @staticmethod
    async def get(session: AsyncSession, event_id: str) -> Optional[EventLog]:
        """
        Get event log by id.
        """
        result = await session.execute(
            select(EventLog).where(EventLog.id == event_id)
        )
        return result.scalar_one_or_none()

    @staticmethod
    async def list(session: AsyncSession, limit: int = 100) -> List[EventLog]:
        """
        Get latest N events (default 100).
        """
        result = await session.execute(
            select(EventLog).order_by(EventLog.ts.desc()).limit(limit)
        )
        return result.scalars().all()

    @staticmethod
    async def delete(session: AsyncSession, event_id: str) -> None:
        """
        Delete event log entry by id.
        """
        obj = await EventLogRepository.get(session, event_id)
        if obj:
            await session.delete(obj)
            await session.commit()

    @staticmethod
    async def list_by_unit(session: AsyncSession, unit_id: str, limit: int = 100) -> List[EventLog]:
        result = await session.execute(
            select(EventLog)
            .where(EventLog.unit_id == unit_id)
            .order_by(EventLog.ts.desc())
            .limit(limit)
        )
        return result.scalars().all()