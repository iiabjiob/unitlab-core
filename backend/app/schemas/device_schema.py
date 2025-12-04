from __future__ import annotations

from datetime import datetime
from typing import Optional, List

from pydantic import BaseModel, ConfigDict, Field, field_validator

from app.schemas.channel_schema import ChannelSchema


def _coerce_timestamp(value: Optional[int | datetime]) -> Optional[int]:
    if value is None:
        return None
    if isinstance(value, datetime):
        return int(value.timestamp() * 1000)
    return value


class DeviceBase(BaseModel):
    unit_id: str
    device_type: Optional[str] = None
    num_channels: Optional[int] = None
    firmware_version: Optional[str] = None
    name: Optional[str] = None

    model_config = ConfigDict(populate_by_name=True)


class DeviceUpdate(BaseModel):
    name: Optional[str] = None


class DeviceSchema(DeviceBase):
    id: int

    status: str = "offline"
    last_seen: Optional[int] = None
    registered_at: Optional[int] = None
    channels: Optional[List["ChannelSchema"]] = None

    model_config = ConfigDict(from_attributes=True, populate_by_name=True)

    @field_validator("last_seen", "registered_at", mode="before")
    @classmethod
    def _normalize_timestamp(cls, value: Optional[int | datetime]) -> Optional[int]:
        return _coerce_timestamp(value)


class DeviceSummary(DeviceBase):
    id: int
    status: str = "offline"
    last_seen: Optional[int] = None
    registered_at: Optional[int] = None

    model_config = ConfigDict(from_attributes=True, populate_by_name=True)

    @field_validator("last_seen", "registered_at", mode="before")
    @classmethod
    def _normalize_timestamp(cls, value: Optional[int | datetime]) -> Optional[int]:
        return _coerce_timestamp(value)


class DeviceBulkDeletePayload(BaseModel):
    ids: list[int] = Field(default_factory=list, min_length=1)

    model_config = ConfigDict(extra="forbid")


class DeviceBulkDeleteResult(BaseModel):
    deleted: int