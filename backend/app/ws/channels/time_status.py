from app.services.system.time_sync_service import TimeSyncService

from app.schemas.ws.events import WSChannel


def get_channel_config():
    return {
        "name": WSChannel.TIME_STATUS,
        "enabled": True,
        "interval": 30,
        "provider": lambda: {
            "timestamp": TimeSyncService.get_current_time_utc(),
            "status": TimeSyncService.get_sync_status(),
            "source": TimeSyncService.get_sync_source(),
            "offset_us": TimeSyncService.get_time_offset_us()
        }
    }
