from datetime import datetime
from typing import ClassVar, Literal

from pydantic import AliasChoices, BaseModel, ConfigDict, Field


SequenceStepTypeLiteral = Literal[
    "WAIT",
    "DO_LATCH",
    "DO_PULSE",
    "DO_PAIR",
    "DO_BITMASK",
    "AO_SET",
    "CALL_SEQUENCE",
    "REPEAT_SEQUENCE",
]


class SequenceStepBase(BaseModel):
    sequence_step_type: SequenceStepTypeLiteral = Field(
        validation_alias=AliasChoices("sequence_step_type", "type", "kind"),
        serialization_alias="sequence_step_type",
    )
    channel_id: int | None = None
    payload: dict[str, object] | None = None

    model_config: ClassVar[ConfigDict] = ConfigDict(populate_by_name=True)


class SequenceStepCreateSchema(BaseModel):
    sequence_step_type: SequenceStepTypeLiteral = Field(
        validation_alias=AliasChoices("sequence_step_type", "type", "kind"),
        serialization_alias="sequence_step_type",
    )
    channel_id: int | None = None
    payload: dict[str, object] | None = None

    model_config: ClassVar[ConfigDict] = ConfigDict(populate_by_name=True)


class SequenceStepUpdateSchema(BaseModel):
    order_index: int | None = None
    sequence_step_type: SequenceStepTypeLiteral | None = Field(
        default=None,
        validation_alias=AliasChoices("sequence_step_type", "type", "kind"),
        serialization_alias="sequence_step_type",
    )
    channel_id: int | None = None
    payload: dict[str, object] | None = None

    model_config: ClassVar[ConfigDict] = ConfigDict(populate_by_name=True)


class SequenceStepSchema(SequenceStepBase):
    id: int
    sequence_id: int
    order_index: int
    created_at: datetime
    updated_at: datetime

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True, populate_by_name=True)


class SequenceReorderSchema(BaseModel):
    new_order: list[int]
