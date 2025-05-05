from sqlalchemy import text
from app.db.database import engine
from app.ws.task_manager import task_manager
from app.core.utils import enable_ap_mode
from app.core.logger import get_logger

from app.mqtt.client import start_mqtt, stop_mqtt
from app.mqtt.client import client
from app.mqtt.topics import device_register_topic, state_all, states_all

logger = get_logger("core")


async def check_database_connection():
    try:
        async with engine.begin() as conn:
            await conn.execute(text("SELECT 1"))
            logger.info("✅ Connected to the database!")
    except Exception as e:
        logger.error(f"❌ Database connection failed: {e}")


def start_background_tasks():
    try:
        enable_ap_mode()
        task_manager.start_tasks()
        logger.info("✅ Background tasks started.")
    except Exception as e:
        logger.error(f"❌ Failed to start background tasks: {e}")


def start_mqtt_client(loop):
    try:
        start_mqtt(loop)

        client.subscribe(device_register_topic())  # device/register/#
        client.subscribe(states_all())       # +/states
        client.subscribe(state_all())        # +/state/#

        logger.info("📡 Subscribed to default MQTT topics")

    except Exception as e:
        logger.error(f"❌ Failed to start MQTT client: {e}")


def stop_mqtt_client():
    try:
        stop_mqtt()
    except Exception as e:
        logger.error(f"❌ Failed to stop MQTT client: {e}")
