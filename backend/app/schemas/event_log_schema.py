from pydantic import BaseModel
from typing import Optional, Any
from datetime import datetime

class EventLogSchema(BaseModel):
    id: str
    ts: int
    dir: str
    source: str
    channel_or_action: str
    unit_id: Optional[str]
    type: Optional[str]
    summary: str
    payload: Optional[Any]
    created_at: datetime

    class Config:
        from_attributes = True
