from datetime import datetime
from typing import List, Literal, Optional

from pydantic import BaseModel, ConfigDict, Field

from app.schemas.channel_schema import ChannelSchema

TestRunStatusLiteral = Literal["pending", "running", "completed", "failed", "cancelled"]


class TestRunSettings(BaseModel):
    delay_ms: int = Field(default=500, ge=0, description="Delay between channel actions in ms")


class TestRunBase(BaseModel):
    name: str


class TestRunCreateSchema(TestRunBase):
    channel_ids: List[int] = Field(default_factory=list)
    settings: Optional[TestRunSettings] = None


class TestRunUpdateSchema(BaseModel):
    name: Optional[str] = None
    settings: Optional[TestRunSettings] = None


class TestRunSummarySchema(TestRunBase):
    id: int
    project_id: int
    status: TestRunStatusLiteral
    created_at: datetime
    updated_at: datetime
    started_at: Optional[datetime] = None
    finished_at: Optional[datetime] = None
    current_step_index: int
    error_message: Optional[str] = None
    settings: TestRunSettings

    model_config = ConfigDict(from_attributes=True)


class TestRunSchema(TestRunSummarySchema):
    steps: List["TestRunStepSchema"] = Field(default_factory=list)


class TestRunStepCreateSchema(BaseModel):
    channel_id: int


class TestRunStepUpdateSchema(BaseModel):
    order_index: Optional[int] = None
    channel_id: Optional[int] = None


class TestRunStepBulkCreateSchema(BaseModel):
    channel_ids: List[int] = Field(default_factory=list)


class TestRunStepSchema(BaseModel):
    id: int
    test_run_id: int
    order_index: int
    channel_id: Optional[int] = None
    created_at: datetime
    updated_at: datetime
    channel: Optional[ChannelSchema] = None

    model_config = ConfigDict(from_attributes=True)


class TestRunReorderSchema(BaseModel):
    new_order: List[int]


class TestRunStateSchema(BaseModel):
    id: int
    status: TestRunStatusLiteral
    started_at: Optional[datetime]
    finished_at: Optional[datetime]
    current_step_index: int
    total_steps: int
    error_message: Optional[str] = None
