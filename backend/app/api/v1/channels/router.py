from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException, Query, Response
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.services.channel_service import ChannelService
from app.schemas.channel_schema import ChannelSchema, ChannelListResponse, ChannelUpdate

router = APIRouter(prefix="/api/v1/channels", tags=["Channels"])


def get_channel_service(db: AsyncSession = Depends(get_db)) -> ChannelService:
    return ChannelService(db)


@router.get("", response_model=ChannelListResponse | list[ChannelSchema])
async def list_channels(
    response: Response,
    limit: int | None = Query(default=None, ge=1, le=1000),
    offset: int = Query(default=0, ge=0),
    service: ChannelService = Depends(get_channel_service),
):
    if limit is None:
        return await service.list()
    payload = await service.list_paginated(limit, offset)
    response.headers.setdefault("X-Total-Count", str(payload.total))
    return payload


@router.get("/{channel_id}", response_model=ChannelSchema)
async def get_channel(channel_id: int, service: ChannelService = Depends(get_channel_service)):
    channel = await service.get(channel_id)
    if not channel:
        raise HTTPException(status_code=404, detail="Channel not found")
    return channel


@router.patch("/{channel_id}", response_model=ChannelSchema)
async def update_channel(
    channel_id: int,
    patch: ChannelUpdate,
    service: ChannelService = Depends(get_channel_service),
):
    channel = await service.update(channel_id, patch.model_dump(exclude_unset=True))
    if not channel:
        raise HTTPException(status_code=404, detail="Channel not found")
    return channel


@router.delete("/{channel_id}")
async def delete_channel(channel_id: int, service: ChannelService = Depends(get_channel_service)):
    deleted = await service.delete(channel_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Channel not found")
    return {"detail": "Channel deleted"}
