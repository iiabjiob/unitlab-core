import time, uuid
from app.infrastructure.protocol.packet_io import PacketParser
from app.infrastructure.protocol.decode import sys as sys_decode
from app.infrastructure.protocol.utils import fw_u16_to_str
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.db.database import AsyncSessionLocal
from app.repositories.device_repository import register_or_update
from app.infrastructure.mqtt import topics
from app.infrastructure.redis.manager import RedisManager
from app.ws.manager import WebSocketManager
from app.schemas.ws.events import DeviceRegisterEvent
from app.services.event_log_service import EventLogService
from app.schemas.ws.events import WSChannel, EventDirection, EventSource
from app.core.logger import get_logger

logger = get_logger("dev")


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

    unit_id = reg.id
    type = reg.type.strip()   # 4-char code, лучше str.strip()
    channels = reg.channels
    firmware_version = fw_u16_to_str(reg.fwVersion)

    logger.debug(
        f"Registering device {unit_id} (type={type}, ch={channels}, fw={firmware_version})"
    )

    async with AsyncSessionLocal() as session:
        try:
            device = await register_or_update(
                db=session,
                unit_id=unit_id,
                channels=channels,
                firmware_version=firmware_version,
                type=type,
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
    await ws_manager.broadcast(event)

    # Дополнительно: логируем событие в event_log
    async with AsyncSessionLocal() as session:
        await EventLogService.log_and_broadcast(session, {
            "id": str(uuid.uuid4()),
            "ts": int(time.time() * 1000),
            "dir": EventDirection.IN,
            "source": EventSource.WS_DEVICE,
            "channel_or_action": WSChannel.DEVICE_REGISTER,
            "unit_id": unit_id,
            "type": type,
            "summary": f"Device registered (ch={channels}, fw={firmware_version})",
            "payload": event.model_dump(),
        })
