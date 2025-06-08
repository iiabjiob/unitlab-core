from app.mqtt.handler_registry import registry
from app.ws.websocket_manager import ws_manager
from app.ws.ws_channels import unit_states_channel
from app.core.protocol import parse_state_payload
from app.core.logger import get_logger

logger = get_logger("mqtt")

@registry.mqtt_handler("unitlab/devices/+/+/heartbeat")
async def handle_device_heartbeat(topic: str, payload: bytes, match):
    device_type, unit_id = match.group(1), match.group(2)
    
    logger.info(f"Handle state: {topic}")

    # 

    logger.debug(
        f"IN ← {device_type.upper()} {unit_id}: "
        f"heartbeat"
    )
    await ws_manager.broadcast(unit_states_channel(unit_id, device_type), {
        "unit_id": unit_id,
        "device_type": device_type,
        
    })
