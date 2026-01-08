from __future__ import annotations

from typing import Optional

from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.datapoints.repository import DatapointRepository
from app.schemas.datapoint_schema import DatapointListResponse, DatapointSchema


class DatapointService:
    """Business helpers for datapoints."""

    def __init__(self, db: AsyncSession):
        self.repo = DatapointRepository(db)

    async def list_paginated(self, project_id: int, limit: int, offset: int) -> DatapointListResponse:
        items, total = await self.repo.list_paginated(project_id, limit, offset)
        payload = [DatapointSchema.model_validate(item) for item in items]
        return DatapointListResponse(items=payload, total=total, limit=limit, offset=offset)

    async def list_all(self, project_id: int) -> list[DatapointSchema]:
        items = await self.repo.list(project_id)
        return [DatapointSchema.model_validate(item) for item in items]

    async def get(self, project_id: int, datapoint_id: int) -> Optional[DatapointSchema]:
        datapoint = await self.repo.get(project_id, datapoint_id)
        return DatapointSchema.model_validate(datapoint) if datapoint else None

    async def create(self, project_id: int, data: dict) -> DatapointSchema:
        datapoint = await self.repo.create(project_id, data)
        return DatapointSchema.model_validate(datapoint)

    async def update(self, project_id: int, datapoint_id: int, changes: dict) -> Optional[DatapointSchema]:
        datapoint = await self.repo.update(project_id, datapoint_id, changes)
        return DatapointSchema.model_validate(datapoint) if datapoint else None

    async def delete(self, project_id: int, datapoint_id: int) -> bool:
        return await self.repo.delete(project_id, datapoint_id)
