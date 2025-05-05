# app/ws/channels/system_info.py

from app.services.system.system_info_service import SystemInfoService
from app.services.system.resource_service import ResourceUsageService
from app.services.system.temperature_service import TemperatureService
from app.services.system.wifi_service import WifiService
from app.ws.ws_channels import system_info_channel

channel = system_info_channel()

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
