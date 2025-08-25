# app/ws/channels/system_info.py

from app.services.system_info_service import SystemInfoService
from app.services.resource_service import ResourceUsageService
from app.services.temperature_service import TemperatureService
from app.services.wifi_service import WifiService
from app.ws.channels.names import SYSTEM_INFO

channel = SYSTEM_INFO

def get_channel_config():
    return {
        "name": channel,
        "enabled": True,
        "interval": 5,
        "provider": lambda: {
            "app_version": SystemInfoService.get_app_version(),
            "host_name": SystemInfoService.get_hostname(),
            "ip_address": SystemInfoService.get_ip_addresses(),
            "cpu": ResourceUsageService.get_cpu_usage(),
            "ram": ResourceUsageService.get_ram_usage(),
            "disk": ResourceUsageService.get_disk_usage(),
            "os": SystemInfoService.get_os(),
            "uptime": SystemInfoService.get_uptime(),
            "temperature": TemperatureService.get_temperature(),
            "wifi": WifiService.get_wifi_info(),
        }
    }
