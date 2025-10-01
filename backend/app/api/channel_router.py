# app/api/channel_router.py
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.schemas.channel_schema import ChannelSchema, ChannelUpdateSchema
from app.repositories.channel_repository import ChannelRepository

router = APIRouter(prefix="/api/channels", tags=["Channels"])


@router.get("", response_model=list[ChannelSchema])
async def get_channels(db: AsyncSession = Depends(get_db)):
    repo = ChannelRepository(db)
    channels = await repo.get_all()
    return [ChannelSchema.model_validate(ch) for ch in channels]


@router.get("/{channel_id}", response_model=ChannelSchema)
async def get_channel(channel_id: int, db: AsyncSession = Depends(get_db)):
    repo = ChannelRepository(db)
    channel = await repo.get_by_id(channel_id)
    if not channel:
        raise HTTPException(status_code=404, detail="Channel not found")
    return ChannelSchema.model_validate(channel)


@router.patch("/{channel_id}", response_model=ChannelSchema)
async def update_channel(
    channel_id: int,
    patch: ChannelUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = ChannelRepository(db)
    channel = await repo.update(channel_id, **patch.model_dump(exclude_unset=True))
    if not channel:
        raise HTTPException(status_code=404, detail="Channel not found")
    return ChannelSchema.model_validate(channel)


@router.delete("/{channel_id}", summary="Delete channel")
async def delete_channel(channel_id: int, db: AsyncSession = Depends(get_db)):
    repo = ChannelRepository(db)
    deleted = await repo.delete(channel_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Channel not found")
    return {"detail": "Channel deleted"}
