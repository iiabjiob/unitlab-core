from app.infrastructure.protocol.packet_io import PacketParser
from app.infrastructure.protocol.decode import sys as sys_decode
from app.infrastructure.protocol.utils import fw_u16_to_str
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.db.database import AsyncSessionLocal
from app.services.device_service import DeviceService
from app.infrastructure.mqtt import topics
from app.infrastructure.redis.manager import RedisManager
from app.core.events.ws_event_publisher import WsEventPublisher
from app.schemas.ws.events import DeviceRegisterEvent
from app.core.logger import get_logger

logger = get_logger("dev")


@registry.mqtt_handler(topics.DEVICE_REGISTER)
async def handle_device_register(topic: str, payload: bytes, unit_id: str):
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
    type_ = reg.type.strip()   # 4-char code
    num_channels = reg.num_channels
    firmware_version = fw_u16_to_str(reg.fwVersion)

    logger.debug(
        f"Registering device {unit_id} (type={type_}, ch={num_channels}, fw={firmware_version})"
    )

    device = None
    created = False
    async with AsyncSessionLocal() as session:
        service = DeviceService(session)
        try:
            device, created = await service.register_or_update(
                unit_id=unit_id,
                num_channels=num_channels,
                firmware_version=firmware_version,
                device_type=type_,
            )
            logger.info(f"✅ Registered device: {device.unit_id}")
        except Exception as e:
            await session.rollback()
            logger.error(f"💥 DB error registering device '{unit_id}': {e}")
            return

    if device is None:
        return

    # enrich with Redis dynamic info
    redis_client = RedisManager.get_instance()
    last_seen = await redis_client.get(f"device:{unit_id}:last_seen")
    status = "online" if last_seen else "offline"

    payload = device.model_dump()
    payload["status"] = status
    payload["last_seen"] = int(last_seen) if last_seen else device.last_seen
    payload["registered_at"] = device.registered_at
    payload["created"] = created

    event = DeviceRegisterEvent(**payload)

    await WsEventPublisher.publish(event)
