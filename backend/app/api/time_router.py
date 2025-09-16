from fastapi import APIRouter
from datetime import datetime, timezone
from app.schemas.time import TimeStatus
from app.services.time_sync import get_chrony_status

router = APIRouter(prefix="/api", tags=["Time"])

@router.get("/time", response_model=TimeStatus)
async def get_time():
    """Return current server time in UTC"""
    now = datetime.now(timezone.utc)
    status, source, offset_us = get_chrony_status()
    return TimeStatus(
        timestamp=now,
        status=status,
        source=source,
        offset_us=offset_us,
    )
