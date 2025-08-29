from pydantic import BaseModel
from typing import Literal, Optional
from datetime import datetime

class TimeStatus(BaseModel):
    timestamp: datetime        # ISO8601 UTC
    status: Literal["synced", "unsynced"]
    source: Optional[str] = None
    offset_us: Optional[int] = None