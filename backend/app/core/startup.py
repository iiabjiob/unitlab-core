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

# Redis healthcheck
async def check_redis_connection():
    global redis_client
    try:
        # Ping Redis
        pong = await redis_client.ping()
        if pong:
            logger.info("✅ Connected to Redis!")
        else:
            logger.error("❌ Redis ping failed!")
    except Exception as e:
        logger.error(f"❌ Redis connection failed: {e}")

# Redis Lifecycle
async def start_redis():
    global redis_client
    redis_client = redis.from_url(
        settings.redis_url, 
        encoding="utf-8", 
        decode_responses=True
    )
    await check_redis_connection()

async def stop_redis():
    global redis_client
    if redis_client:
        await redis_client.close()

# MQTT Client Lifecycle
async def start_mqtt_client():
    global mqtt_client
    unitId = "unitlab-core"
    mqtt_client = UnitLabMqttClient.get_instance(client_id=unitId)
    await mqtt_client.connect(host=settings.mqtt_host, port=settings.mqtt_port)
    await mqtt_client.connected.wait()
    logger.info(f"📡 MQTT client '{unitId}' connected and subscribed.")

async def stop_mqtt_client():
    global mqtt_client
    if mqtt_client:
        await mqtt_client.disconnect()