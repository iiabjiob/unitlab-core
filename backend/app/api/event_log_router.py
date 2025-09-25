# app/api/event_log_router.py
from fastapi import APIRouter, Depends, Query, HTTPException
from typing import Optional, List
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.schemas.event_log_schema import EventLogSchema
from app.repositories.event_log_repository import EventLogRepository

router = APIRouter(prefix="/api/events", tags=["EventLog"])


@router.get("", response_model=List[EventLogSchema])
async def get_events(
    limit: int = Query(100, ge=1, le=1000, description="Number of events to fetch"),
    unit_id: Optional[str] = Query(None, description="Filter by device unit_id"),
    channel_id: Optional[int] = Query(None, description="Filter by channel_id"),
    db: AsyncSession = Depends(get_db),
):
    """
    Get last N events (optionally filtered by unit_id or channel_id).
    """
    repo = EventLogRepository(db)

    if channel_id:
        events = await repo.list_by_channel(channel_id=channel_id, limit=limit)
    elif unit_id:
        events = await repo.list_by_unit(unit_id=unit_id, limit=limit)
    else:
        events = await repo.list(limit=limit)

    return events


@router.get("/{event_id}", response_model=EventLogSchema)
async def get_event(event_id: int, db: AsyncSession = Depends(get_db)):
    """Get event by ID."""
    repo = EventLogRepository(db)
    event = await repo.get(event_id)
    if not event:
        raise HTTPException(status_code=404, detail="Event not found")
    return event


@router.delete("/{event_id}")
async def delete_event(event_id: int, db: AsyncSession = Depends(get_db)):
    """Delete event by ID."""
    repo = EventLogRepository(db)
    ok = await repo.delete(event_id)
    if not ok:
        raise HTTPException(status_code=404, detail="Event not found")
    return {"detail": "Event deleted"}
