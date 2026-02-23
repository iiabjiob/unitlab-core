from app.infrastructure.protocol.packet_io import PacketParser
from app.infrastructure.protocol.decode import bit as bit_decode, afloat as float_decode
from app.infrastructure.protocol.modes import State
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.mqtt import topics
from app.services.device_state_service import DeviceStateService
from app.core.events.ws_event_publisher import WsEventPublisher
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
    elif hdr.mode == State.DIAG_ALL_BIT:
        decoded = bit_decode.state_diag(body)
    elif hdr.mode == State.STATE_CHANGED_BIT:
        decoded = bit_decode.state_delta(body)
    elif hdr.mode in (State.DIAG_DI_BIT, State.DIAG_DI_BIT_V2):
        decoded = bit_decode.state_diag_di(body)
    elif hdr.mode == State.STATE_LATCHED_BIT:
        decoded = bit_decode.state_latched(body)
    elif hdr.mode == State.STATE_SINGLE_FLOAT:
        decoded = float_decode.state_single(body)
    elif hdr.mode == State.DIAG_AO_FLOAT:
        decoded = float_decode.diag_all(body)
    else:
        decoded = None

    if not decoded:
        logger.error(f"💥 Failed to decode STATE from {unit_id}, mode=0x{hdr.mode:02X}")
        return

    changed, event = await DeviceStateService.update_state(unit_id, hdr, decoded)

    if not event:
        logger.debug(f"⏩ No state change for {unit_id}, skip WS broadcast")
        return

    logger.debug(f"📥 IN ← {unit_id}: {event.model_dump()}")

    await WsEventPublisher.publish(event)
