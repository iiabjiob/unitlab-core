import time
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.mqtt import topics
from app.ws.manager import WebSocketManager
from app.schemas.ws.events import DeviceHeartbeatEvent
from app.core.config import get_settings
from app.core.logger import get_logger

settings = get_settings()
logger = get_logger("mqtt")

@registry.mqtt_handler(topics.DEVICE_HEARTBEAT)
async def handle_device_heartbeat(topic: str, payload: bytes, unit_id: str):
    
    ts = int(time.time() * 1000)
    logger.debug(f"📥 IN ← {unit_id}: heartbeat @ {ts}")

    redis = RedisManager.get_instance()

    # обновляем last_seen (только он с TTL)
    await redis.set(
        f"device:{unit_id}:last_seen",
        str(ts).encode(),
        ex=settings.heartbeat_ttl
    )

    # регистрируем девайс в all (если впервые)
    await redis.sadd("devices:all", unit_id.encode())

    # проверяем статус
    prev_status = await redis.get(f"device:{unit_id}:status")
    if prev_status is not None:
        prev_status = prev_status.decode() if isinstance(prev_status, (bytes, bytearray)) else str(prev_status)

    if prev_status != "online":
        await redis.set(f"device:{unit_id}:status", "online")
        
        event = DeviceHeartbeatEvent(
                unit_id=unit_id,
                status="online",
                last_seen=int(time.time() * 1000),
            )
        ws_manager = WebSocketManager.get_instance()
        await ws_manager.broadcast(event)

        logger.info(f"Device {unit_id} came online")