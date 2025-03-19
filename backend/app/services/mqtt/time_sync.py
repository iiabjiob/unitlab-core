import asyncio
from app.services.system.time_service import TimeSyncService
from app.core.logger import logger
from app.core.config import get_settings
from app.services.mqtt.mqtt_client import MQTTClient  # Используем новый класс

settings = get_settings()

# Создаём MQTT-клиент с топиком "time_sync"
mqtt_time_sync = MQTTClient(topic="time_sync")

async def time_sync_updater(interval: int):
    """Фоновая задача для отправки обновлений времени через MQTT с заданным интервалом."""
    logger.info(f"✅ Background task 'time_sync_updater' started with interval {interval}s.")

    last_sent_data = None  # Сохраняем последнее отправленное сообщение

    while True:
        ptp_sync = TimeSyncService.get_ptp_status()
        ntp_sync = TimeSyncService.get_ntp_status()

        if ptp_sync:
            time_data = {"source": "PTP", "synchronized": True, "current_time": TimeSyncService.get_ptp_time()}
        elif ntp_sync:
            time_data = {"source": "NTP", "synchronized": True, "current_time": TimeSyncService.get_ntp_time()}
        else:
            time_data = {"source": "*", "synchronized": False, "current_time": None}

        # Отправляем только если данные изменились
        if time_data != last_sent_data:
            mqtt_time_sync.publish(time_data)  # Используем объект mqtt_time_sync
            last_sent_data = time_data
            logger.debug(f"📡 MQTT Time sync sent: {time_data}")
        else:
            logger.debug("⏳ No changes in time sync data, skipping MQTT publish.")

        await asyncio.sleep(interval)  # Интервал задается параметром
