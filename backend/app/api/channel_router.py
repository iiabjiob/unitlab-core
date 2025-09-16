# app/api/channel_router.py
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.infrastructure.db.database import get_db
from app.models.channel import Channel
from app.schemas.channel_schema import ChannelSchema, ChannelUpdateSchema

router = APIRouter(prefix="/api/channels", tags=["Channels"])

@router.get("", response_model=list[ChannelSchema])
async def get_channels(db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Channel))
    channels = result.scalars().all()
    return [ChannelSchema.model_validate(ch) for ch in channels]

@router.get("/{channel_id}", response_model=ChannelSchema)
async def get_channel(channel_id: int, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Channel).where(Channel.id == channel_id))
    channel = result.scalar_one_or_none()

    if not channel:
        raise HTTPException(status_code=404, detail="Channel not found")

    return ChannelSchema.model_validate(channel)

@router.patch("/{channel_id}", response_model=ChannelSchema)
async def update_channel(
    channel_id: int,
    patch: ChannelUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    result = await db.execute(select(Channel).where(Channel.id == channel_id))
    channel = result.scalar_one_or_none()

    if not channel:
        raise HTTPException(status_code=404, detail="Channel not found")

    for field, value in patch.model_dump(exclude_unset=True).items():
        setattr(channel, field, value)


    await db.commit()
    await db.refresh(channel)

    return ChannelSchema.model_validate(channel)

@router.delete("/{channel_id}", summary="Delete channel")
async def delete_channel(channel_id: int, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Channel).where(Channel.id == channel_id))
    channel = result.scalar_one_or_none()

    if not channel:
        raise HTTPException(status_code=404, detail="Channel not found")

    await db.delete(channel)
    await db.commit()

    return {"detail": "Channel deleted"}
