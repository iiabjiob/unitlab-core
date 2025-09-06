import uuid
from app.infrastructure.protocol.packet_io import PacketParser
from app.infrastructure.protocol.decode import sys as sys_decode
from app.infrastructure.protocol.packet_structures import RespStatus, RespError
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.mqtt import topics
from app.ws.manager import WebSocketManager
from app.schemas.ws.events import DeviceRespEvent, WSChannel, EventDirection, EventSource
from app.services.event_log_service import EventLogService
from app.infrastructure.db.database import AsyncSessionLocal
from app.core.logger import get_logger

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

        # Build WS event
        event = DeviceRespEvent(
            unit_id=unit_id,
            packet_id=parser.hdr.packet_id,
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

    ws_manager = WebSocketManager.get_instance()
    await ws_manager.broadcast(event)

    # Логируем в event_log (DB + WS events/log)
    async with AsyncSessionLocal() as session:
        await EventLogService.log_and_broadcast(session, {
            "id": str(uuid.uuid4()),
            "ts": parser.hdr.timestamp_ms,
            "dir": EventDirection.IN,
            "source": EventSource.WS_DEVICE,
            "channel_or_action": WSChannel.DEVICE_RESP,
            "unit_id": unit_id,
            "summary": f"RESP packetId={parser.hdr.packet_id} status={status.name} err={error.name}",
            "payload": event.model_dump(),
        })
