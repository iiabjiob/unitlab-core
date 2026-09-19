from typing import ClassVar

from datetime import datetime

from pydantic import BaseModel, ConfigDict, Field


class ChannelBase(BaseModel):
    channel_index: int = Field(alias="index")
    channel_type: str = Field(alias="type")
    name: str | None = None

    model_config: ClassVar[ConfigDict] = ConfigDict(populate_by_name=True)


class ChannelSchema(ChannelBase):
    id: int
    device_id: int
    resolved_name: str | None = None
    created_at: datetime
    updated_at: datetime

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True, populate_by_name=True)


class ChannelListItem(ChannelSchema):
    pass


class ChannelListResponse(BaseModel):
    items: list[ChannelListItem]
    total: int
    limit: int
    offset: int


class ChannelUpdate(BaseModel):
    channel_index: int | None = Field(default=None, alias="index", ge=0)
    channel_type: str | None = Field(default=None, alias="type")
    name: str | None = None

    model_config: ClassVar[ConfigDict] = ConfigDict(populate_by_name=True)
