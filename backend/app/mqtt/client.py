import asyncio
import paho.mqtt.client as mqtt
from app.core.config import get_settings
from app.core.logger import get_logger

settings = get_settings()
logger = get_logger("mqtt")

MQTT_HOST = settings.mqtt_host
MQTT_PORT = settings.mqtt_port

client = mqtt.Client()
_loop = None  # Приватный event loop внутри модуля


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        logger.info("✅ Connected successfully")
    else:
        logger.error(f"❌ Connection failed with code {rc}")


def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode()

    logger.debug(f"📥 Received: {topic} → {payload}")


def start_mqtt(loop: asyncio.AbstractEventLoop):
    """
    Инициализация MQTT клиента и установка внешнего event loop для асинхронной доставки.
    """
    global _loop
    _loop = loop

    try:
        client.on_connect = on_connect
        client.on_message = on_message

        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        client.loop_start()

        logger.info("🚀 Client started")
    except Exception as e:
        logger.exception(f"❌ Startup error: {e}")


def stop_mqtt():
    try:
        client.loop_stop()
        client.disconnect()
        logger.info("🛑 Client stopped")
    except Exception as e:
        logger.exception(f"❌ Shutdown error: {e}")
