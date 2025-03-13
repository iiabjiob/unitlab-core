from fastapi import APIRouter
from app.services.time_service import TimeSyncService

router = APIRouter(prefix="/time", tags=["Time Synchronization"])

@router.get("/status")
def time_status():
    """ Проверяет синхронизацию NTP и PTP """
    return {
        "ntp": {
            "synchronized": TimeSyncService.get_ntp_status(),
            "current_time": TimeSyncService.get_ntp_time()
        },
        "ptp": {
            "synchronized": TimeSyncService.get_ptp_status() != "Ошибка получения статуса PTP",
            "current_time": TimeSyncService.get_ptp_time()
        }
    }
