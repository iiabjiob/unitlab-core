from __future__ import annotations

from datetime import datetime
from typing import Optional

from pydantic import AliasChoices, BaseModel, ConfigDict, Field

from app.schemas.allocation_schema import AllocationSchema


class DatapointBase(BaseModel):
    datapoint_code: str = Field(validation_alias=AliasChoices("datapoint_code", "code"))
    datapoint_type: str = Field(validation_alias=AliasChoices("datapoint_type", "type"))
    name: str
    hmi_text: Optional[str] = None
    voltage_level: Optional[str] = None
    bay: Optional[str] = None
    ied_name: Optional[str] = None
    terminal: Optional[str] = None

    model_config = ConfigDict(populate_by_name=True)


class DatapointCreate(DatapointBase):
    """Payload for creating datapoints."""


class DatapointUpdate(BaseModel):
    datapoint_code: Optional[str] = Field(default=None, validation_alias=AliasChoices("datapoint_code", "code"))
    datapoint_type: Optional[str] = Field(default=None, validation_alias=AliasChoices("datapoint_type", "type"))
    name: Optional[str] = None
    hmi_text: Optional[str] = None
    voltage_level: Optional[str] = None
    bay: Optional[str] = None
    ied_name: Optional[str] = None
    terminal: Optional[str] = None

    model_config = ConfigDict(populate_by_name=True)


class DatapointSchema(DatapointBase):
    id: int
    project_id: int
    allocation: Optional[AllocationSchema] = None
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True, populate_by_name=True)


class DatapointListResponse(BaseModel):
    items: list[DatapointSchema]
    total: int
    limit: int
    offset: int
