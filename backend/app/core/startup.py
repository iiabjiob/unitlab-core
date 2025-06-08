import app.mqtt.handlers  # ← импортируем все хандлеры
import asyncio
from sqlalchemy import text
from app.db.database import engine

from app.core.logger import get_logger
from app.core.config import get_settings

import redis.asyncio as redis

from app.mqtt.gmqtt_client import UnitLabMqttClient

settings = get_settings()

logger = get_logger("core")

redis_client: redis.Redis = None
mqtt_client: UnitLabMqttClient = None

# Database healthcheck
async def check_database_connection():
    try:
        async with engine.begin() as conn:
            await conn.execute(text("SELECT 1"))
            logger.info("✅ Connected to the database!")
    except Exception as e:
        logger.error(f"❌ Database connection failed: {e}")