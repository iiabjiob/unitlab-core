from sqlalchemy.sql import text
from app.db.database import engine
from app.mqtt.client import start_mqtt
from app.ws.task_manager import task_manager
from app.core.logger import get_logger

logger = get_logger("core")

async def check_database_connection():
    try:
        async with engine.begin() as conn:
            await conn.execute(text("SELECT 1"))
            logger.info("✅ Connected to the database!")
    except Exception as e:
        logger.error(f"❌ Database connection failed: {e}")

def mqtt_connect():
    try:
        start_mqtt()
    except Exception as e:
        logger.error(f"❌ Failed to start mqtt: {e}")


def start_background_tasks():
    try:
        task_manager.start_tasks()
        logger.info("✅ Background tasks started.")
    except Exception as e:
        logger.error(f"❌ Failed to start background tasks: {e}")
