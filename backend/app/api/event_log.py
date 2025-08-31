from fastapi import APIRouter, Depends, Query
from sqlalchemy.ext.asyncio import AsyncSession
from typing import Optional, List

from app.infrastructure.db.database import get_db
from app.schemas.event_log_schema import EventLogSchema
from app.repositories.event_log_repository import EventLogRepository

router = APIRouter(prefix="/api", tags=["EventLog"])


@router.get("/events", response_model=List[EventLogSchema])
async def get_events(
    limit: int = Query(100, ge=1, le=1000, description="Number of events to fetch"),
    unit_id: Optional[str] = Query(None, description="Filter by device unit_id"),
    db: AsyncSession = Depends(get_db),
):
    """
    Get last N events (optionally filtered by unit_id).
    """
    if unit_id:
        events = await EventLogRepository.list_by_unit(db, unit_id=unit_id, limit=limit)
    else:
        events = await EventLogRepository.list(db, limit=limit)

    return events
