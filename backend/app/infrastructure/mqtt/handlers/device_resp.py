from app.infrastructure.protocol.packet_io import PacketParser
from app.infrastructure.protocol.decode import sys as sys_decode
from app.infrastructure.protocol.packet_structures import RespStatus, RespError
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.mqtt import topics
from app.ws.manager import WebSocketManager
from app.schemas.ws.events import DeviceRespEvent, WSChannel
from app.core.logger import get_logger

logger = get_logger("mqtt")


@registry.mqtt_handler(topics.DEVICE_RESP)
async def handle_device_resp(topic: str, payload: bytes, match):
    device_type, unit_id = match.group(1), match.group(2)

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

        # Build WS event
        event = DeviceRespEvent(
            unit_id=unit_id,
            device_type=device_type,
            packet_id=parser.hdr.packet_id,
            status=status,
            error=error,
            timestamp=parser.hdr.timestamp_ms,
        )

    except Exception as e:
        logger.error(f"💥 Exception while decoding RESP from {unit_id}: {e}")
        return

    logger.debug(
        f"IN ← {device_type.upper()} {unit_id}: RESP packetId={parser.hdr.packet_id} "
        f"status={status.name} err={error.name}"
    )

    ws_manager = WebSocketManager.get_instance()
    await ws_manager.broadcast(WSChannel.DEVICE_RESP, event.model_dump())
