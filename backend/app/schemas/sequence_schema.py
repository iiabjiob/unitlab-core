from datetime import datetime
from typing import Any, Dict, List, Optional

from pydantic import AliasChoices, BaseModel, ConfigDict, Field

from app.schemas.sequence_step_schema import (
    SequenceStepSchema,
    SequenceStepCreateSchema,
)


class SequenceBase(BaseModel):
    name: str
    description: Optional[str] = None


class SequenceCreateSchema(SequenceBase):
    steps: List[SequenceStepCreateSchema] = Field(default_factory=list)


class SequenceUpdateSchema(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None


class SequenceSchema(SequenceBase):
    id: int
    created_at: datetime
    updated_at: datetime
    steps: List[SequenceStepSchema] = Field(default_factory=list)

    model_config = ConfigDict(from_attributes=True)


class SequenceExportStepSchema(BaseModel):
    order_index: int
    sequence_step_type: str = Field(
        validation_alias=AliasChoices("sequence_step_type", "type", "kind"),
        serialization_alias="sequence_step_type",
    )
    unit_id: Optional[str] = None
    channel_index: Optional[int] = None
    payload: Optional[Dict[str, Any]] = None


class SequenceExportSchema(BaseModel):
    name: str
    description: Optional[str] = None
    steps: List[SequenceExportStepSchema] = Field(default_factory=list)
