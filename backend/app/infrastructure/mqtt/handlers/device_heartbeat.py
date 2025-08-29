from app.infrastructure.mqtt.handler_registry import registry
from app.ws.manager import WebSocketManager
from app.infrastructure.protocol.header import unpack_header
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.mqtt import topics
from app.schemas.ws.events import DeviceHeartbeatEvent, WSChannel
from app.core.logger import get_logger

logger = get_logger("mqtt")


@registry.mqtt_handler(topics.DEVICE_HEARTBEAT)
async def handle_device_heartbeat(topic: str, payload: bytes, match):
    device_type, unit_id = match.group(1), match.group(2)

    # Parse header to extract device timestamp
    hdr = unpack_header(payload)
    ts = hdr.timestamp_ms

    logger.debug(f"📤 IN ← {device_type.upper()} {unit_id}: heartbeat @ {ts}")

    redis_client = RedisManager.get_instance()
    ws_manager = WebSocketManager.get_instance()

    # Update status in Redis
    await redis_client.sadd("devices:online", unit_id)
    await redis_client.set(f"device:{unit_id}:last_seen", ts, ex=60)
    await redis_client.set(f"device:{unit_id}:type", device_type)

    # Build WS event
    event = DeviceHeartbeatEvent(
        unit_id=unit_id,
        device_type=device_type,
        status="online",
        last_seen=ts,
    )

    # Broadcast WS
    await ws_manager.broadcast(WSChannel.DEVICE_STATE, event.model_dump())
