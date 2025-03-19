import asyncio
from app.services.mqtt.mqtt_client import MQTTClient
from app.core.logger import logger
from app.core.config import get_settings

settings = get_settings()
CHECK_INTERVAL = settings.health_check_interval
mqtt_health = MQTTClient(topic="health_status")

async def health_status_updater():
    """Периодически отправляет статус FastAPI через MQTT."""
    logger.info(f"✅ Запуск 'health_status_updater' (интервал: {CHECK_INTERVAL}s)")

    while True:
        status_data = {"status": "online", "message": "FastAPI is running"}
        mqtt_health.publish(status_data)  # Отправляем статус через MQTT
        logger.debug(f"📡 MQTT sent the status: {status_data}")

        await asyncio.sleep(CHECK_INTERVAL)  # Ждем перед следующим обновлением
