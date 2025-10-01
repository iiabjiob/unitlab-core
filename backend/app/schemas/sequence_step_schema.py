from pydantic import BaseModel
from typing import List, Optional, Dict


class SequenceStepBase(BaseModel):
    kind: str
    channel_id: Optional[int] = None
    payload: Optional[Dict] = None


class SequenceStepCreateSchema(SequenceStepBase):
    pass


class SequenceStepUpdateSchema(BaseModel):
    order_index: Optional[int] = None
    kind: Optional[str] = None
    channel_id: Optional[int] = None
    payload: Optional[Dict] = None


class SequenceStepSchema(SequenceStepBase):
    id: int
    sequence_id: int
    order_index: int

    class Config:
        from_attributes = True


class SequenceReorderSchema(BaseModel):
    new_order: List[int]
