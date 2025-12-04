from __future__ import annotations

from typing import List, Optional

from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.channels.repository import ChannelRepository
from app.schemas.channel_schema import (
    ChannelSchema,
    ChannelListItem,
    ChannelListResponse,
)


class ChannelService:
    """High-level channel operations used by routers and other services."""

    def __init__(self, db: AsyncSession):
        self.repo = ChannelRepository(db)

    async def list(self) -> List[ChannelSchema]:
        channels = await self.repo.list()
        return [ChannelSchema.model_validate(ch) for ch in channels]

    async def list_paginated(self, limit: int, offset: int) -> ChannelListResponse:
        channels, total = await self.repo.list_paginated(limit, offset)
        items = [ChannelListItem.model_validate(ch) for ch in channels]
        return ChannelListResponse(items=items, total=total, limit=limit, offset=offset)

    async def list_by_device(self, device_id: int) -> List[ChannelSchema]:
        channels = await self.repo.list_by_device(device_id)
        return [ChannelSchema.model_validate(ch) for ch in channels]

    async def list_by_device_paginated(self, device_id: int, limit: int, offset: int) -> ChannelListResponse:
        channels, total = await self.repo.list_by_device_paginated(device_id, limit, offset)
        items = [ChannelListItem.model_validate(ch) for ch in channels]
        return ChannelListResponse(items=items, total=total, limit=limit, offset=offset)

    async def get(self, channel_id: int) -> Optional[ChannelSchema]:
        channel = await self.repo.get(channel_id)
        return ChannelSchema.model_validate(channel) if channel else None

    async def update(self, channel_id: int, changes: dict) -> Optional[ChannelSchema]:
        allowed = {"name", "channel_index", "channel_type"}
        payload = {key: value for key, value in changes.items() if key in allowed}
        if not payload:
            channel = await self.repo.get(channel_id)
        else:
            channel = await self.repo.update(channel_id, payload)
        return ChannelSchema.model_validate(channel) if channel else None

    async def delete(self, channel_id: int) -> bool:
        return await self.repo.delete(channel_id)
