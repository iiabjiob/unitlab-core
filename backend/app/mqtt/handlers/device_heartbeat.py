import time
from app.mqtt.handler_registry import registry
from app.ws.ws_manager import WebSocketManager
from app.redis.redis_manager import RedisManager
from app.ws.ws_channels import unit_states_channel
from app.core.logger import get_logger

logger = get_logger("mqtt")

@registry.mqtt_handler("unitlab/devices/+/+/heartbeat")
async def handle_device_heartbeat(topic: str, payload: bytes, match):
    device_type, unit_id = match.group(1), match.group(2)
    
    logger.info(f"Handle state: {topic}")

    redis_client = RedisManager.get_instance()
    ws_manager = WebSocketManager.get_instance()

    # Сохраняем онлайн-статус и тип в Redis с TTL
    await redis_client.set(f"device:{unit_id}:online", int(time.time()), ex=60)
    await redis_client.set(f"device:{unit_id}:type", device_type, ex=120)

    logger.debug(
        f"IN ← {device_type.upper()} {unit_id}: "
        f"heartbeat"
    )
    # Пушим на фронт (всем подписчикам на этот канал)
    await ws_manager.broadcast(unit_states_channel(unit_id, device_type), {
        "unit_id": unit_id,
        "device_type": device_type,
        "status": "online",
        "timestamp": int(time.time()),
    })
