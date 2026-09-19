from __future__ import annotations

from typing import ClassVar

from datetime import datetime

from pydantic import BaseModel, ConfigDict, Field, field_validator

from app.schemas.channel_schema import ChannelSchema


def _coerce_timestamp(value: int | datetime | None) -> int | None:
    if value is None:
        return None
    if isinstance(value, datetime):
        return int(value.timestamp() * 1000)
    return value


class DeviceBase(BaseModel):
    unit_id: str
    device_type: str | None = None
    num_channels: int | None = None
    firmware_version: str | None = None
    name: str | None = None

    model_config: ClassVar[ConfigDict] = ConfigDict(populate_by_name=True)


class DeviceUpdate(BaseModel):
    name: str | None = None


class DeviceSchema(DeviceBase):
    id: int

    status: str = "offline"
    last_seen: int | None = None
    registered_at: int | None = None
    channels: list["ChannelSchema"] | None = None

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True, populate_by_name=True)

    @field_validator("last_seen", "registered_at", mode="before")
    @classmethod
    def _normalize_timestamp(cls, value: int | datetime | None) -> int | None:
        return _coerce_timestamp(value)


class DeviceSummary(DeviceBase):
    id: int
    status: str = "offline"
    last_seen: int | None = None
    registered_at: int | None = None

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True, populate_by_name=True)

    @field_validator("last_seen", "registered_at", mode="before")
    @classmethod
    def _normalize_timestamp(cls, value: int | datetime | None) -> int | None:
        return _coerce_timestamp(value)


class DeviceBulkDeletePayload(BaseModel):
    ids: list[int] = Field(default_factory=list, min_length=1)

    model_config: ClassVar[ConfigDict] = ConfigDict(extra="forbid")


class DeviceBulkDeleteResult(BaseModel):
    deleted: int