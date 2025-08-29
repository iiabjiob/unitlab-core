from app.infrastructure.protocol.packet_io import PacketParser
from app.infrastructure.protocol.decode import sys as sys_decode
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.db.database import AsyncSessionLocal
from app.repositories.device_repository import register_if_not_exists
from app.infrastructure.mqtt import topics
from app.infrastructure.redis.manager import RedisManager
from app.ws.manager import WebSocketManager
from app.schemas.ws.events import DeviceRegisterEvent, WSChannel
from app.core.logger import get_logger

logger = get_logger("device")


@registry.mqtt_handler(topics.DEVICE_REGISTER)
async def handle_device_register(topic: str, payload: bytes, match):
    """
    Handle device registration messages from MQTT.
    """
    try:
        parser = PacketParser(payload)
        if not parser.parse_header():
            logger.error(f"💥 Invalid packet header in {topic}")
            return

        reg = sys_decode.register_msg(parser.payload())
        if not reg:
            logger.error(f"💥 Failed to decode REGISTER payload from {topic}")
            return
    except Exception as e:
        logger.error(f"💥 Exception while decoding REGISTER: {e}")
        return

    unit_id = reg["device_id"]
    device_type = reg["device_type"]
    channels = reg["channels"]
    firmware_version = reg["fw_version"]

    logger.debug(
        f"Registering device {unit_id} (type={device_type}, ch={channels}, fw={firmware_version})"
    )

    async with AsyncSessionLocal() as session:
        try:
            device = await register_if_not_exists(
                db=session,
                unit_id=unit_id,
                channels=channels,
                firmware_version=firmware_version,
                type=device_type,
                is_active=True,
            )
            logger.info(f"✅ Registered device: {device.unit_id}")
        except Exception as e:
            await session.rollback()
            logger.error(f"💥 DB error registering device '{unit_id}': {e}")
            return

    # enrich with Redis dynamic info
    redis_client = RedisManager.get_instance()
    last_seen = await redis_client.get(f"device:{unit_id}:last_seen")
    status = "online" if last_seen else "offline"

    # Build WS event
    event = DeviceRegisterEvent(
        unit_id=device.unit_id,
        type=device.type,
        channels=device.channels,
        location=device.location,
        firmware_version=device.firmware_version,
        is_active=device.is_active,
        status=status,
        last_seen=int(last_seen) if last_seen else None,
    )

    ws_manager = WebSocketManager.get_instance()
    await ws_manager.broadcast(WSChannel.DEVICE_REGISTER, event.model_dump())
