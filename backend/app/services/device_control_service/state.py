from app.infrastructure.mqtt import topics
from app.infrastructure.protocol.packet_io import PacketBuilder
from app.infrastructure.protocol.modes import State
from app.infrastructure.mqtt.gmqtt_client import UnitLabMqttClient
from app.core.logger import get_logger

logger = get_logger("mqtt")

def request_state_now(device_type: str, unit_id: str, ch: int = None, float_type=False):
    
    topic = topics.req_state(device_type, unit_id)
    
    if float_type:
        if ch is None:
            mode = State.REQ_ALL_FLOAT
            payload = b""
        else:
            mode = State.REQ_SINGLE_FLOAT
            payload = bytes([ch])
    else:
        if ch is None:
            mode = State.REQ_ALL_BIT
            payload = b""
        else:
            mode = State.REQ_SINGLE_BIT
            payload = bytes([ch])

    builder = PacketBuilder()
    builder.build(mode, packet_id=0, ts=0, payload=payload)
    data = builder.to_bytes()

    logger.info(f"📤 OUT → {topic} | payload={data.hex().upper()} ({mode.name})")
    UnitLabMqttClient.get_instance().publish(topic, data)
