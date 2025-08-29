from dataclasses import asdict
from app.infrastructure.protocol.packet_io import PacketParser
from app.infrastructure.protocol.decode import bit as bit_decode, afloat as float_decode
from app.infrastructure.protocol.modes import State
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.mqtt import topics
from app.ws.manager import WebSocketManager
from app.schemas.ws.events import DeviceStateEvent, device_state
from app.core.logger import get_logger

logger = get_logger("mqtt")


@registry.mqtt_handler(topics.DEVICE_STATE)
async def handle_device_state(topic: str, payload: bytes, match):
    device_type, unit_id = match.group(1), match.group(2)

    parser = PacketParser(payload)
    if not parser.parse_header():
        logger.error(f"💥 Invalid STATE packet from {unit_id}")
        return

    hdr = parser.hdr
    body = parser.payload()

    decoded = None
    if hdr.mode == State.STATE_SINGLE_BIT:
        decoded = bit_decode.state_single(body)
    elif hdr.mode == State.STATE_ALL_BIT:
        decoded = bit_decode.state_all(body)
    elif hdr.mode == State.STATE_SINGLE_FLOAT:
        decoded = float_decode.state_single(body)

    if not decoded:
        logger.error(f"💥 Failed to decode STATE from {unit_id}, mode=0x{hdr.mode:02X}")
        return

    # Build WS event
    event = DeviceStateEvent(
        unit_id=unit_id,
        device_type=device_type,
        timestamp=hdr.timestamp_ms,
        mode=State(hdr.mode),
        payload=asdict(decoded),
    )

    logger.debug(f"IN ← {device_type.upper()} {unit_id}: {event.model_dump()}")

    ws_manager = WebSocketManager.get_instance()
    await ws_manager.broadcast(device_state(device_type, unit_id), event.model_dump())
