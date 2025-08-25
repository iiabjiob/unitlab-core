import time
from app.infrastructure.mqtt.handler_registry import registry
from app.ws.manager import WebSocketManager
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.mqtt import topics
from app.core.logger import get_logger

logger = get_logger("mqtt")

@registry.mqtt_handler(topics.DEVICE_HEARTBEAT)
async def handle_device_heartbeat(topic: str, payload: bytes, match):
    device_type, unit_id = match.group(1), match.group(2)
    
    logger.info(f"Handle state: {topic}")

    redis_client = RedisManager.get_instance()
    ws_manager = WebSocketManager.get_instance()

    # Обновляем статус online
    await redis_client.sadd("devices:online", unit_id)
    await redis_client.set(f"device:{unit_id}:last_seen", int(time.time()), ex=60)

    # Обновляем тип (без TTL)
    await redis_client.set(f"device:{unit_id}:type", device_type)

    logger.debug(
        f"IN ← {device_type.upper()} {unit_id}: "
        f"heartbeat"
    )
    await ws_manager.broadcast("devices/status", {
        "unit_id": unit_id,
        "device_type": device_type,
        "status": "online",
        "timestamp": int(time.time())
    })
