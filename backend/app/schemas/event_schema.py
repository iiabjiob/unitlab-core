from __future__ import annotations

from datetime import datetime
from typing import Any, Literal, Optional, TYPE_CHECKING

from pydantic import AliasChoices, BaseModel, ConfigDict, Field

from app.schemas.channel_schema import ChannelSchema
from app.schemas.device_schema import DeviceSchema

if TYPE_CHECKING:  # pragma: no cover
    from app.schemas.sequence_step_schema import SequenceStepSchema  # noqa: F401

EventType = Literal["cmd", "state", "system", "sequence", "status"]
EventDirection = Literal["in", "out"]
EventResult = Literal["ok", "error", "timeout", "pending"]


class EventBase(BaseModel):
    event_type: EventType = Field(validation_alias=AliasChoices("event_type", "type"))
    source: str = "system"
    direction: Optional[EventDirection] = None
    result: Optional[EventResult] = None
    payload: Optional[dict[str, Any]] = None
    message: Optional[str] = None

    model_config = ConfigDict(populate_by_name=True)


class EventCreate(EventBase):
    project_id: Optional[int] = None
    ts: Optional[datetime] = None
    packet_id: Optional[str] = None
    datapoint_id: Optional[int] = None
    device_id: Optional[int] = None
    channel_id: Optional[int] = None


class EventSchema(EventBase):
    id: int
    ts: datetime
    project_id: Optional[int] = None
    packet_id: Optional[str] = None
    datapoint_id: Optional[int] = None
    device_id: Optional[int] = None
    channel_id: Optional[int] = None

    channel: Optional[ChannelSchema] = None
    device: Optional[DeviceSchema] = None

    model_config = ConfigDict(
        from_attributes=True,
        populate_by_name=True,
        ser_json_datetime="iso8601",
    )
