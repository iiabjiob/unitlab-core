from fastapi import APIRouter
from datetime import datetime, timezone
from app.schemas.time import TimeStatus

router = APIRouter(prefix="/api", tags=["Time"])

@router.get("/time", response_model=TimeStatus)
async def get_time():
    """Return current server time in UTC"""
    now = datetime.now(timezone.utc)
    return TimeStatus(
        timestamp=now,
        status="unsynced",
        source="local",
        offset_us=None,
    )
