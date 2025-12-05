from __future__ import annotations

from typing import List

from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.schemas.event_schema import EventCreate, EventSchema
from app.services.event_service import EventService

router_system = APIRouter(prefix="/api/v1/events", tags=["Events"])
router_project = APIRouter(prefix="/api/v1/projects/{project_id}/events", tags=["Events"])


@router_system.get("", response_model=List[EventSchema])
async def get_system_events(
    limit: int = Query(100, le=1000),
    cursor: int | None = Query(None, ge=1),
    db: AsyncSession = Depends(get_db),
):
    service = EventService(db)
    return await service.list(None, limit=limit, include_system=False, cursor=cursor)


@router_project.get("", response_model=List[EventSchema])
async def get_project_events(
    project_id: int,
    limit: int = Query(100, le=1000),
    cursor: int | None = Query(None, ge=1),
    include_system: bool = Query(True),
    db: AsyncSession = Depends(get_db),
):
    service = EventService(db)
    return await service.list(project_id, limit=limit, include_system=include_system, cursor=cursor)


@router_project.get("/{event_id}", response_model=EventSchema)
async def get_event(project_id: int, event_id: int, db: AsyncSession = Depends(get_db)):
    service = EventService(db)
    event = await service.get(project_id, event_id)
    if not event:
        raise HTTPException(status_code=404, detail="Event not found")
    return event


@router_project.post("", response_model=EventSchema)
async def create_event(project_id: int, body: EventCreate, db: AsyncSession = Depends(get_db)):
    service = EventService(db)
    data = body.model_dump()
    data.setdefault("project_id", project_id)
    return await service.create(data)


@router_project.delete("/{event_id}")
async def delete_event(project_id: int, event_id: int, db: AsyncSession = Depends(get_db)):
    service = EventService(db)
    deleted = await service.delete(project_id, event_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Event not found")
    return {"detail": "Event deleted"}
