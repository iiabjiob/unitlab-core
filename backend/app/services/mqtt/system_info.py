import asyncio
from app.services.mqtt.mqtt_client import MQTTClient
from app.services.system.system_info_service import SystemInfoService
from app.services.system.resource_usage_service import ResourceUsageService
from app.services.system.temperature_service import TemperatureService
from app.services.system.wifi_service import WifiService
from app.core.logger import logger
from app.core.config import get_settings

settings = get_settings()
UPDATE_INTERVAL = 2  # Update system info every 2 seconds

mqtt_system_info = MQTTClient(topic="system_info")  # MQTT client for system info

async def system_info_updater():
    """Background task that sends system updates via MQTT."""

    logger.info(f"✅ Background task 'system_info_updater' started (interval: {UPDATE_INTERVAL}s).")

    last_sent_data = None  # Store last sent message

    while True:
        data = {
            "uptime": SystemInfoService.get_uptime(),
            "cpu_usage": ResourceUsageService.get_cpu_usage(),
            "ram": ResourceUsageService.get_ram_usage(),
            "disk": ResourceUsageService.get_disk_usage(),
            "temperature": TemperatureService.get_temperature(),
            "wifi": WifiService.get_wifi_info(),
        }

        # Publish only if data has changed
        if data != last_sent_data:
            mqtt_system_info.publish(data)
            last_sent_data = data  # Store last sent data
            logger.debug(f"📡 System info sent to MQTT: {data}")
        else:
            logger.debug("⏳ System info has not changed, skipping MQTT publish.")

        await asyncio.sleep(UPDATE_INTERVAL)
