from pydantic import BaseModel


class SequenceStepBase(BaseModel):
    order_index: int
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

    class Config:
        from_attributes = True
