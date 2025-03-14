from fastapi import APIRouter
from app.services.time_service import TimeSyncService
from app.core.logger import logger

router = APIRouter(prefix="/api/time", tags=["Time Synchronization"])

@router.get("/status")
def time_status():
    """ Checks synchronization status and retrieves time from the best available source. """
    ptp_sync = TimeSyncService.get_ptp_status()
    ntp_sync = TimeSyncService.get_ntp_status()

    if ptp_sync:
        logger.info("✅ Time synchronized via PTP.")
        return {
            "source": "PTP",
            "synchronized": True,
            "current_time": TimeSyncService.get_ptp_time()
        }

    if ntp_sync:
        logger.info("✅ Time synchronized via NTP.")
        return {
            "source": "NTP",
            "synchronized": True,
            "current_time": TimeSyncService.get_ntp_time()
        }

    logger.warning("⚠️ Time is not synchronized.")
    return {
        "source": "*",
        "synchronized": False,
        "current_time": None  # ❌ Теперь нет смысла передавать NTP время, если нет синхронизации
    }
