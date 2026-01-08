from __future__ import annotations

from typing import Optional

from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.allocations.repository import AllocationRepository
from app.api.v1.datapoints.repository import DatapointRepository
from app.schemas.allocation_schema import AllocationSchema


class AllocationError(ValueError):
    """Base class for allocation validation errors."""


class AllocationConflictError(AllocationError):
    """Raised when a requested allocation conflicts with another datapoint."""


class DatapointNotFoundError(AllocationError):
    """Raised when the referenced datapoint does not exist for the project."""


class ChannelNotFoundError(AllocationError):
    """Raised when the target channel cannot be found."""


class AllocationService:
    def __init__(self, db: AsyncSession):
        self.repo = AllocationRepository(db)
        self.datapoints = DatapointRepository(db)

    async def get_for_datapoint(self, project_id: int, datapoint_id: int) -> Optional[AllocationSchema]:
        allocation = await self.repo.get_by_datapoint(project_id, datapoint_id)
        return AllocationSchema.model_validate(allocation) if allocation else None

    async def assign(self, project_id: int, datapoint_id: int, channel_id: int) -> AllocationSchema:
        datapoint = await self.datapoints.get(project_id, datapoint_id)
        if not datapoint:
            raise DatapointNotFoundError("Datapoint not found")

        channel = await self.repo.get_channel(channel_id)
        if not channel:
            raise ChannelNotFoundError("Channel not found")

        conflict = await self.repo.get_by_channel(project_id, channel_id)
        if conflict and conflict.datapoint_id != datapoint_id:
            raise AllocationConflictError("Channel is already allocated to another datapoint")

        existing = await self.repo.get_by_datapoint(project_id, datapoint_id)
        if existing:
            existing.channel_id = channel_id
            await self.repo.update(existing)
        else:
            await self.repo.create(
                {
                    "project_id": project_id,
                    "datapoint_id": datapoint_id,
                    "channel_id": channel_id,
                }
            )

        refreshed = await self.repo.get_by_datapoint(project_id, datapoint_id)
        if not refreshed:
            raise RuntimeError("Failed to load allocation after assignment")
        return AllocationSchema.model_validate(refreshed)

    async def clear(self, project_id: int, datapoint_id: int) -> bool:
        allocation = await self.repo.get_by_datapoint(project_id, datapoint_id)
        if not allocation:
            return False
        await self.repo.delete(allocation)
        return True
