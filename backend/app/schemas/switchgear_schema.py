# app/schemas/switchgear_schema.py
from pydantic import BaseModel, ConfigDict
from typing import Optional

class SwitchgearSchema(BaseModel):
    id: int
    title: str
    kind: str
    do_open: Optional[int] = None
    do_closed: Optional[int] = None
    di_open: Optional[int] = None
    di_close: Optional[int] = None
    feedback_delay_ms: int

    model_config = ConfigDict(from_attributes=True, extra="ignore")

class SwitchgearCreateSchema(BaseModel):
    title: str
    kind: str = "switchgear"
    do_open: Optional[int] = None
    do_closed: Optional[int] = None
    di_open: Optional[int] = None
    di_close: Optional[int] = None
    feedback_delay_ms: int = 0

class SwitchgearUpdateSchema(BaseModel):
    title: Optional[str] = None
    kind: Optional[str] = None
    do_open: Optional[int] = None
    do_closed: Optional[int] = None
    di_open: Optional[int] = None
    di_close: Optional[int] = None
    feedback_delay_ms: Optional[int] = None
