import time
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.mqtt import topics
from app.core.config import get_settings
from app.core.logger import get_logger

settings = get_settings()
logger = get_logger("mqtt")

@registry.mqtt_handler(topics.DEVICE_HEARTBEAT)
async def handle_device_heartbeat(topic: str, payload: bytes, match):
    type, unit_id = match.group(1), match.group(2)

    # Use server timestamp (ms since epoch)
    ts = int(time.time() * 1000)

    logger.debug(f"📥 IN ← {type.upper()} {unit_id}: heartbeat @ {ts}")

    redis_client = RedisManager.get_instance()
    
    # Update status in Redis
    await redis_client.sadd("devices:online", unit_id.encode())

    await redis_client.set(
        f"device:{unit_id}:last_seen",
        str(ts).encode(),
        ex=settings.heartbeat_ttl
    )

    await redis_client.set(
        f"device:{unit_id}:type",
        type.encode()
    )
    await redis_client.set(
        f"device:{unit_id}:status",
        b"online",
        ex=settings.heartbeat_ttl
    )