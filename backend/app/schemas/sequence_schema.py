from pydantic import BaseModel, ConfigDict
from typing import Optional, List, Dict, Any
from app.schemas.sequence_step_schema import SequenceStepSchema


class SequenceSchema(BaseModel):
    id: int
    name: str
    description: Optional[str]
    steps: List[SequenceStepSchema]

    model_config = ConfigDict(from_attributes=True)


# шаг при создании через API (мы всё равно сразу мапим на channel_id)
class SequenceCreateStepSchema(BaseModel):
    order_index: int
    kind: str
    channel_id: Optional[int] = None
    payload: Optional[Dict[str, Any]] = None


class SequenceCreateSchema(BaseModel):
    name: str
    description: Optional[str] = None
    steps: List[SequenceCreateStepSchema] = []


class SequenceUpdateSchema(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None


# шаг для экспорта/импорта (внешние ID)
class SequenceExportStepSchema(BaseModel):
    order_index: int
    kind: str
    unit_id: Optional[str] = None        # device.unit_id
    channel_index: Optional[int] = None  # channel.index внутри устройства
    payload: Optional[Dict[str, Any]] = None


class SequenceExportSchema(BaseModel):
    name: str
    description: Optional[str] = None
    steps: List[SequenceExportStepSchema] = []
