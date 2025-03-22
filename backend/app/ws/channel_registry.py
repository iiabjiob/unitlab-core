# app/ws/channel_registry.py

from app.services.system.system_info_service import SystemInfoService
from app.services.system.resource_service import ResourceUsageService
from app.services.system.temperature_service import TemperatureService
from app.services.system.wifi_service import WifiService
from app.services.system.time_sync_service import TimeSyncService

# Каналы
SYSTEM_INFO = "system_info"
TIME_STATUS = "time_status"

CHANNELS = {
    SYSTEM_INFO: {
        "enabled": True,
        "interval": 5,
        "provider": lambda: {
            "app_version": SystemInfoService.get_app_version(),
            "host_name": SystemInfoService.get_hostname(),
            "ip_address": SystemInfoService.get_ip_address(),
            "cpu": ResourceUsageService.get_cpu_usage(),
            "ram": ResourceUsageService.get_ram_usage(),
            "disk": ResourceUsageService.get_disk_usage(),
            "os": SystemInfoService.get_os(),
            "uptime": SystemInfoService.get_uptime(),
            "temperature": TemperatureService.get_temperature(),
            "wifi": WifiService.get_wifi_info(),
        }
    },
    TIME_STATUS: {
        "enabled": True,
        "interval": 30,
        "provider": lambda: {
            "timestamp": TimeSyncService.get_current_time_utc(),
            "status": TimeSyncService.get_sync_status(),
            "source": TimeSyncService.get_sync_source(),
            "offset_us": TimeSyncService.get_time_offset_us()
        }
    }
}
