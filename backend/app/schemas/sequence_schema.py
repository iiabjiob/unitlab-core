from datetime import datetime
from typing import ClassVar

from pydantic import AliasChoices, BaseModel, ConfigDict, Field

from app.schemas.sequence_step_schema import (
    SequenceStepSchema,
    SequenceStepCreateSchema,
)


class SequenceBase(BaseModel):
    name: str
    description: str | None = None


class SequenceCreateSchema(SequenceBase):
    steps: list[SequenceStepCreateSchema] = Field(default_factory=list)


class SequenceUpdateSchema(BaseModel):
    name: str | None = None
    description: str | None = None


class SequenceSchema(SequenceBase):
    id: int
    created_at: datetime
    updated_at: datetime
    system_key: str | None = None
    system_provided: bool
    read_only: bool
    workspace_ids: list[int] = Field(default_factory=list)
    steps: list[SequenceStepSchema] = Field(default_factory=list)

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True)


class SequenceExportStepSchema(BaseModel):
    order_index: int
    sequence_step_type: str = Field(
        validation_alias=AliasChoices("sequence_step_type", "type", "kind"),
        serialization_alias="sequence_step_type",
    )
    unit_id: str | None = None
    channel_index: int | None = None
    payload: dict[str, object] | None = None


class SequenceExportSchema(BaseModel):
    name: str
    description: str | None = None
    steps: list[SequenceExportStepSchema] = Field(default_factory=list)
