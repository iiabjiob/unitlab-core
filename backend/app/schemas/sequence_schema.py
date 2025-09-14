from pydantic import BaseModel, ConfigDict
from typing import Optional, List, Dict, Any


class SequenceStepSchema(BaseModel):
    id: int
    order_index: int
    kind: str
    unit_id: Optional[str] = None
    payload: Optional[Dict[str, Any]] = None

    model_config = ConfigDict(from_attributes=True)


class SequenceSchema(BaseModel):
    id: int
    name: str
    description: Optional[str]
    steps: List[SequenceStepSchema]

    model_config = ConfigDict(from_attributes=True)


class SequenceCreateStepSchema(BaseModel):
    order_index: int
    kind: str
    unit_id: Optional[str] = None
    payload: Optional[Dict[str, Any]] = None


class SequenceCreateSchema(BaseModel):
    name: str
    description: Optional[str] = None
    steps: List[SequenceCreateStepSchema]


class SequenceUpdateSchema(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None

class SequenceExportSchema(BaseModel):
    name: str
    description: Optional[str] = None
    steps: List[SequenceCreateStepSchema]