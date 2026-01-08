from __future__ import annotations

from datetime import datetime
from typing import Optional

from pydantic import BaseModel, ConfigDict, Field

from app.schemas.channel_schema import ChannelSchema


class AllocationSchema(BaseModel):
    """Allocation with optional channel context."""

    id: int
    project_id: int
    datapoint_id: int
    channel_id: Optional[int] = Field(default=None)
    created_at: datetime
    updated_at: datetime
    channel: Optional[ChannelSchema] = None

    model_config = ConfigDict(from_attributes=True)


class AllocationAssignRequest(BaseModel):
    channel_id: int = Field(..., gt=0)


class AllocationListResponse(BaseModel):
    items: list[AllocationSchema]
    total: int
    limit: int
    offset: int
