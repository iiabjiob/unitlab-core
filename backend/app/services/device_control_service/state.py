from app.infrastructure.mqtt import topics
from app.infrastructure.protocol.packet_io import PacketBuilder
from app.infrastructure.protocol.modes import State
from app.infrastructure.mqtt.manager import MqttManager
from app.core.logger import get_logger

logger = get_logger("mqtt")

def request_state_now(type: str, unit_id: str, mode: State, ch: int | None = None):
    topic = topics.req_state(type, unit_id)

    if mode in (State.REQ_SINGLE_BIT, State.REQ_SINGLE_FLOAT):
        if ch is None:
            raise ValueError(f"{mode.name} requires channel number")
        payload = bytes([ch])
    else:
        payload = b""

    builder = PacketBuilder()
    builder.build(mode, packet_id=0, ts=0, payload=payload)
    data = builder.to_bytes()

    logger.info(f"📤 OUT → {topic} | payload={data.hex().upper()} ({mode.name})")
    MqttManager.get_instance().publish(topic, data)
