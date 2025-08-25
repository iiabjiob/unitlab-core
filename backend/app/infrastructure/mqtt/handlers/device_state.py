from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.mqtt import topics
from app.ws.manager import WebSocketManager
from app.ws.channels.names import device_state
from app.core.logger import get_logger

logger = get_logger("mqtt")

@registry.mqtt_handler(topics.DEVICE_STATE)
async def handle_unit_state(topic: str, payload: bytes, match):
    device_type, unit_id = match.group(1), match.group(2)
    payload_str = payload.decode() if isinstance(payload, bytes) else str(payload)
    logger.info(f"Handle state: {topic} | payload: {payload_str}")

    ws_manager = WebSocketManager.get_instance()
    
    try:
        parsed = "TODO BY PROTOCOL"
    except Exception as e:
        logger.error(f"Failed to parse payload: {payload_str}, error: {e}")
        return

    logger.debug(
        f"IN ← {device_type.upper()} {unit_id}: "
        f"{parsed['bitmask']:08X} ({parsed['states']}) "
        f"ts={parsed['timestamp']} ch={parsed['channels']}"
    )
    await ws_manager.broadcast(device_state(unit_id, device_type), {
        "unit_id": unit_id,
        "device_type": device_type,
        "channels": parsed["channels"],
        "states": parsed["states"],
    })
