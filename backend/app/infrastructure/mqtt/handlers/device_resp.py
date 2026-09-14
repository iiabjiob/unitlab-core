from app.infrastructure.protocol.packet_io import PacketParser
from app.infrastructure.protocol.decode import sys as sys_decode
from app.infrastructure.protocol.packet_structures import RespStatus, RespError
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.mqtt import topics
from app.core.events.ws_event_publisher import WsEventPublisher
from app.schemas.ws.events import DeviceRespEvent
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.db.database import AsyncSessionLocal
from app.services.hardware_command_ack import record_hardware_command_ack

logger = get_logger("mqtt")


@registry.mqtt_handler(topics.DEVICE_RESP)
async def handle_device_resp(topic: str, payload: bytes, unit_id: str):
    
    try:
        parser = PacketParser(payload)
        if not parser.parse_header():
            logger.error(f"💥 Invalid RESP packet from {unit_id}")
            return

        resp = sys_decode.resp(parser.payload())
        if not resp:
            logger.error(f"💥 Failed to decode RESP from {unit_id}")
            return

        status = RespStatus(resp.status)
        error = RespError(resp.errCode)

        command_id = None
        try:
            command_id = await RedisManager.get_instance().get(f"hardware:command:{unit_id}:{parser.hdr.packet_id}")
        except Exception:  # noqa: BLE001
            logger.debug("Command correlation lookup unavailable for %s/%s", unit_id, parser.hdr.packet_id)

        if command_id:
            try:
                async with AsyncSessionLocal() as session:
                    await record_hardware_command_ack(
                        session,
                        command_id=str(command_id).strip(),
                        unit_id=unit_id,
                        packet_id=parser.hdr.packet_id,
                        status=status.name,
                        error=error.name,
                    )
            except Exception:  # noqa: BLE001
                logger.exception("Unable to persist command ACK for %s/%s", unit_id, parser.hdr.packet_id)

        # Build WS event
        event = DeviceRespEvent(
            unit_id=unit_id,
            packet_id=parser.hdr.packet_id,
            command_id=str(command_id).strip() if command_id else None,
            status=status,
            error=error,
            timestamp=parser.hdr.timestamp_ms,
        )

    except Exception as e:
        logger.error(f"💥 Exception while decoding RESP from {unit_id}: {e}")
        return

    logger.debug(
        f"📥 IN ← {unit_id}: RESP packetId={parser.hdr.packet_id} "
        f"status={status.name} err={error.name}"
    )

    await WsEventPublisher.publish(event)
