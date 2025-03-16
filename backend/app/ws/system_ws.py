import asyncio
from app.services.websocket_manager import ws_manager
from app.services.system_info_service import SystemInfoService
from app.services.resource_usage_service import ResourceUsageService
from app.services.temperature_service import TemperatureService
from app.services.wifi_service import WifiService
from app.core.logger import logger

async def system_info_updater():
    """ Background task that sends system updates via WebSocket only when clients are connected """

    logger.info("✅ Background task 'system_info_updater' started successfully.")

    last_sent_data = None  # ✅ Запоминаем последнее отправленное сообщение

    while True:
        if ws_manager.has_active_connections('system_info'):
            data = {
                "uptime": SystemInfoService.get_uptime(),
                "cpu_usage": ResourceUsageService.get_cpu_usage(),
                "ram": ResourceUsageService.get_ram_usage(),
                "disk": ResourceUsageService.get_disk_usage(),
                "temperature": TemperatureService.get_temperature(),
                "wifi": WifiService.get_wifi_info(),
            }

            # ✅ Отправляем только если данные изменились
            if data != last_sent_data:
                await ws_manager.send_data("system_info", data)
                last_sent_data = data  # ✅ Запоминаем последние данные
                logger.debug(f"📡 System info sent to WebSocket clients: {data}")
            else:
                logger.debug("⏳ System info has not changed, skipping update.")

        else:
            logger.debug("⏳ No WebSocket clients connected. Waiting...")

        await asyncio.sleep(2)  # Update every 2 seconds
