from pydantic import BaseModel
from typing import List

class SequenceStepBase(BaseModel):
    kind: str
    unit_id: str | None = None
    payload: dict | None = None


class SequenceStepCreateSchema(SequenceStepBase):
    pass


class SequenceStepUpdateSchema(BaseModel):
    order_index: int | None = None
    kind: str | None = None
    unit_id: str | None = None
    payload: dict | None = None


class SequenceStepSchema(SequenceStepBase):
    id: int
    sequence_id: int
    order_index: int

    class Config:
        from_attributes = True

class SequenceReorderSchema(BaseModel):
    new_order: List[int]