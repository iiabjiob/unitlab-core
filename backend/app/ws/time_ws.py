import asyncio
from app.services.websocket_manager import ws_manager
from app.services.time_service import TimeSyncService
from app.core.logger import logger
from app.core.config import get_settings

settings = get_settings()

SYNC_INTERVAL = int (settings.time_sync_interval)

async def time_sync_updater():
    """Фоновая задача для отправки обновлений времени только при наличии клиентов."""
    
    logger.info("✅ Background task 'time_sync_updater' started successfully.")

    last_sent_data = None  # ✅ Сохраняем последнее отправленное сообщение

    while True:
        if ws_manager.has_active_connections("time_sync"):

            ptp_sync = TimeSyncService.get_ptp_status()
            ntp_sync = TimeSyncService.get_ntp_status()

            if ptp_sync:
                time_data = {"source": "PTP", "synchronized": True, "current_time": TimeSyncService.get_ptp_time()}
            elif ntp_sync:
                time_data = {"source": "NTP", "synchronized": True, "current_time": TimeSyncService.get_ntp_time()}
            else:
                time_data = {"source": "*", "synchronized": False, "current_time": None}

            # ✅ Отправляем только если данные изменились
            if time_data != last_sent_data:
                await ws_manager.send_data("time_sync", time_data)
                last_sent_data = time_data  # ✅ Обновляем последнее отправленное сообщение
                logger.debug(f"📡 Time sync data sent: {time_data}")
            else:
                logger.debug("⏳ Time sync data has not changed, skipping update.")

        else:
            logger.debug("⏳ No WebSocket clients connected to 'time_sync'. Waiting...")

        await asyncio.sleep(SYNC_INTERVAL)
