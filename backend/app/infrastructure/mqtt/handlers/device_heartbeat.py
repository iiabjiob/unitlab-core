import time
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.mqtt import topics
from app.core.config import get_settings
from app.core.logger import get_logger

settings = get_settings()
logger = get_logger("mqtt")

@registry.mqtt_handler(topics.DEVICE_HEARTBEAT)
async def handle_device_heartbeat(topic: str, payload: bytes, match=None):
    if match:
        dev_type, unit_id = match.group(1), match.group(2)
    else:
        parts = topic.split("/")
        dev_type, unit_id = parts[2], parts[3]

    ts = int(time.time() * 1000)
    logger.debug(f"📥 IN ← {dev_type.upper()} {unit_id}: heartbeat @ {ts}")

    redis = RedisManager.get_instance()

    # обновляем last_seen (только он с TTL)
    await redis.set(
        f"device:{unit_id}:last_seen",
        str(ts).encode(),
        ex=settings.heartbeat_ttl
    )

    # сохраняем тип
    await redis.set(f"device:{unit_id}:type", dev_type.encode())

    # регистрируем девайс в all (если впервые)
    await redis.sadd("devices:all", unit_id.encode())

    # проверяем статус
    prev_status = await redis.get(f"device:{unit_id}:status")
    if prev_status != "online":
        await redis.set(f"device:{unit_id}:status", "online")
        logger.info(f"Device {unit_id} ({dev_type}) came online")
    
        # TODO: записываем снапшот. Пока этого механизма нет
