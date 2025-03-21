import asyncio
from app.ws.websocket_manager import ws_manager
from app.services.system.resource_service import ResourceUsageService
from app.services.system.system_info_service import SystemInfoService
from app.services.system.temperature_service import TemperatureService
from app.services.system.wifi_service import WifiService
from app.core.config import get_settings

settings = get_settings()

CHANNEL_NAME = "system_info"
INTERVAL = settings.system_info_interval  # Update interval in seconds

async def send_system_info():
    """Send periodic system information via WebSocket"""
    last_sent = None  # Cache to avoid redundant updates

    while True:
        system_info = {
            "cpu": ResourceUsageService.get_cpu_usage(),
            "ram": ResourceUsageService.get_ram_usage(),
            "disk": ResourceUsageService.get_disk_usage(),
            "os": SystemInfoService.get_os(),
            "uptime": SystemInfoService.get_uptime(),
            "temperature": TemperatureService.get_temperature(),
            "wifi": WifiService.get_wifi_info(),
        }

        if system_info != last_sent:  # Avoid redundant sends
            await ws_manager.broadcast(CHANNEL_NAME, system_info)
            last_sent = system_info

        await asyncio.sleep(INTERVAL)
