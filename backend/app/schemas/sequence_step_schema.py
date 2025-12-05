from datetime import datetime
from typing import Any, Dict, List, Literal, Optional

from pydantic import AliasChoices, BaseModel, ConfigDict, Field


SequenceStepTypeLiteral = Literal[
    "WAIT",
    "DO_LATCH",
    "DO_PULSE",
    "DO_PAIR",
    "DO_BITMASK",
    "AO_SET",
]


class SequenceStepBase(BaseModel):
    order_index: int
    sequence_step_type: SequenceStepTypeLiteral = Field(
        validation_alias=AliasChoices("sequence_step_type", "type", "kind"),
        serialization_alias="sequence_step_type",
    )
    channel_id: Optional[int] = None
    payload: Optional[Dict[str, Any]] = None

    model_config = ConfigDict(populate_by_name=True)


class SequenceStepCreateSchema(BaseModel):
    order_index: Optional[int] = None
    sequence_step_type: SequenceStepTypeLiteral = Field(
        validation_alias=AliasChoices("sequence_step_type", "type", "kind"),
        serialization_alias="sequence_step_type",
    )
    channel_id: Optional[int] = None
    payload: Optional[Dict[str, Any]] = None

    model_config = ConfigDict(populate_by_name=True)


class SequenceStepUpdateSchema(BaseModel):
    order_index: Optional[int] = None
    sequence_step_type: Optional[SequenceStepTypeLiteral] = Field(
        default=None,
        validation_alias=AliasChoices("sequence_step_type", "type", "kind"),
        serialization_alias="sequence_step_type",
    )
    channel_id: Optional[int] = None
    payload: Optional[Dict[str, Any]] = None

    model_config = ConfigDict(populate_by_name=True)


class SequenceStepSchema(SequenceStepBase):
    id: int
    sequence_id: int
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True, populate_by_name=True)


class SequenceReorderSchema(BaseModel):
    new_order: List[int]
