from app.mqtt.handler_registry import registry
from app.ws.manager import ws_manager
from app.ws.ws_channels import unit_states_channel
from app.core.protocol import parse_state_payload
from app.core.logger import get_logger

logger = get_logger("mqtt")

@registry.mqtt_handler("unitlab/devices/+/+/state")
async def handle_unit_state(topic: str, payload: bytes, match):
    device_type, unit_id = match.group(1), match.group(2)
    payload_str = payload.decode() if isinstance(payload, bytes) else str(payload)
    logger.info(f"Handle state: {topic} | payload: {payload_str}")

    try:
        parsed = parse_state_payload(payload_str)
    except Exception as e:
        logger.error(f"Failed to parse payload: {payload_str}, error: {e}")
        return

    logger.debug(
        f"IN ← {device_type.upper()} {unit_id}: "
        f"{parsed['bitmask']:08X} ({parsed['states']}) "
        f"ts={parsed['timestamp']} ch={parsed['channels']}"
    )
    await ws_manager.broadcast(unit_states_channel(unit_id, device_type), {
        "unit_id": unit_id,
        "device_type": device_type,
        "channels": parsed["channels"],
        "states": parsed["states"],
    })
