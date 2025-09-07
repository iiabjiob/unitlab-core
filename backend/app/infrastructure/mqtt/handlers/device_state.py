import time, uuid
from app.infrastructure.protocol.packet_io import PacketParser
from app.infrastructure.protocol.decode import bit as bit_decode, afloat as float_decode
from app.infrastructure.protocol.modes import State
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.mqtt import topics
from app.services.device_state_service import DeviceStateService
from app.ws.manager import WebSocketManager
from app.schemas.ws.events import (
    WSChannel,
    EventDirection,
    EventSource,
)
from app.services.event_log_service import EventLogService
from app.infrastructure.db.database import AsyncSessionLocal
from app.core.logger import get_logger

logger = get_logger("mqtt")


@registry.mqtt_handler(topics.DEVICE_STATE)
async def handle_device_state(topic: str, payload: bytes, unit_id: str):

    logger.debug(f"STATE payload len={len(payload)} hex={payload.hex()}")

    parser = PacketParser(payload)
    if not parser.parse_header():
        logger.error(f"💥 Invalid STATE packet from {unit_id}")
        return

    hdr = parser.hdr
    body = parser.payload()

    logger.debug(f"STATE parsed hdr={hdr} body={body.hex()}")

    if hdr.mode == State.STATE_SINGLE_BIT:
        decoded = bit_decode.state_single(body)
    elif hdr.mode == State.STATE_ALL_BIT:
        decoded = bit_decode.state_all(body)
    elif hdr.mode == State.STATE_SINGLE_FLOAT:
        decoded = float_decode.state_single(body)
    else:
        decoded = None

    if not decoded:
        logger.error(f"💥 Failed to decode STATE from {unit_id}, mode=0x{hdr.mode:02X}")
        return

    changed, should_log, event = await DeviceStateService.update_state(unit_id, hdr, decoded)

    # if not changed:
    #     logger.debug(f"⏩ No state change for {unit_id}, skip WS broadcast")
    #     return

    logger.debug(f"📥 IN ← {unit_id}: {event.model_dump()}")

    ws_manager = WebSocketManager.get_instance()
    await ws_manager.broadcast(event)

    if should_log:
        async with AsyncSessionLocal() as session:
            await EventLogService.log_and_broadcast(session, {
                "id": str(uuid.uuid4()),
                "ts": hdr.timestamp_ms or int(time.time() * 1000),
                "dir": EventDirection.IN,
                "source": EventSource.WS_DEVICE,
                "channel_or_action": WSChannel.DEVICE_STATE,
                "unit_id": unit_id,
                "summary": f"STATE update (mode=0x{hdr.mode:02X})",
                "payload": event.model_dump(),
            })
