from __future__ import annotations

from typing import Any, Dict

from sqlalchemy.ext.asyncio import AsyncSession

from app.services.event_service import EventService


class EventLogService:
    """Backward-compatible facade around EventService."""

    @staticmethod
    async def log_and_broadcast(session: AsyncSession, payload: Dict[str, Any]):
        return await EventService.log_and_broadcast(session, payload)