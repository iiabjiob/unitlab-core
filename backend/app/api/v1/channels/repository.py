from __future__ import annotations

from typing import Optional

from sqlalchemy import func, select
from sqlalchemy.orm import load_only
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.channel import Channel
from app.core.logger import get_logger


class ChannelRepository:
    """Thin data-access wrapper for channels."""

    def __init__(self, db: AsyncSession):
        self.db = db
        self.logger = get_logger("channels")

    def _base_query(self):
        return select(Channel).order_by(Channel.id.asc())

    async def list(self) -> list[Channel]:
        result = await self.db.execute(self._base_query())
        return list(result.scalars().all())

    async def list_paginated(self, limit: int, offset: int) -> tuple[list[Channel], int]:
        stmt = (
            select(Channel)
            .options(
                load_only(
                    Channel.id,
                    Channel.device_id,
                    Channel.channel_index,
                    Channel.channel_type,
                    Channel.name,
                    Channel.created_at,
                    Channel.updated_at,
                )
            )
            .order_by(Channel.id.asc())
            .limit(limit)
            .offset(offset)
        )
        result = await self.db.execute(stmt)
        items = list(result.scalars().all())

        total_stmt = select(func.count(Channel.id))
        total_result = await self.db.execute(total_stmt)
        total = int(total_result.scalar_one() or 0)
        return items, total

    async def list_by_device(self, device_id: int) -> list[Channel]:
        stmt = self._base_query().where(Channel.device_id == device_id)
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def list_by_device_paginated(self, device_id: int, limit: int, offset: int) -> tuple[list[Channel], int]:
        stmt = (
            select(Channel)
            .options(
                load_only(
                    Channel.id,
                    Channel.device_id,
                    Channel.channel_index,
                    Channel.channel_type,
                    Channel.name,
                    Channel.created_at,
                    Channel.updated_at,
                )
            )
            .where(Channel.device_id == device_id)
            .order_by(Channel.id.asc())
            .limit(limit)
            .offset(offset)
        )
        result = await self.db.execute(stmt)
        items = list(result.scalars().all())

        total_stmt = select(func.count(Channel.id)).where(Channel.device_id == device_id)
        total_result = await self.db.execute(total_stmt)
        total = int(total_result.scalar_one() or 0)
        return items, total

    async def get(self, channel_id: int) -> Optional[Channel]:
        result = await self.db.execute(self._base_query().where(Channel.id == channel_id))
        return result.scalar_one_or_none()

    async def update(self, channel_id: int, changes: dict) -> Optional[Channel]:
        channel = await self.get(channel_id)
        if not channel:
            return None
        for key, value in changes.items():
            setattr(channel, key, value)
        await self.db.commit()
        await self.db.refresh(channel)
        return channel

    async def delete(self, channel_id: int) -> bool:
        channel = await self.get(channel_id)
        if not channel:
            return False
        await self.db.delete(channel)
        await self.db.commit()
        return True
