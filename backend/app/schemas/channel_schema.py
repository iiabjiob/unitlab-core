from datetime import datetime
from typing import Optional

from pydantic import BaseModel, ConfigDict, Field


class ChannelBase(BaseModel):
    channel_index: int = Field(alias="index")
    channel_type: str = Field(alias="type")
    name: Optional[str] = None

    model_config = ConfigDict(populate_by_name=True)


class ChannelSchema(ChannelBase):
    id: int
    device_id: int
    resolved_name: Optional[str] = None
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True, populate_by_name=True)


class ChannelListItem(ChannelSchema):
    pass


class ChannelListResponse(BaseModel):
    items: list[ChannelListItem]
    total: int
    limit: int
    offset: int


class ChannelUpdate(BaseModel):
    channel_index: Optional[int] = Field(default=None, alias="index", ge=0)
    channel_type: Optional[str] = Field(default=None, alias="type")
    name: Optional[str] = None

    model_config = ConfigDict(populate_by_name=True)
