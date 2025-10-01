from pydantic import BaseModel, ConfigDict
from typing import Optional, Any
from datetime import datetime


class EventLogSchema(BaseModel):
    id: int
    ts: datetime                 # лучше хранить как datetime, а не int
    type: str
    summary: str
    payload: Optional[Any]

    # привязка к каналу
    channel_id: Optional[int] = None
    
    created_at: datetime

    model_config = ConfigDict(from_attributes=True)
