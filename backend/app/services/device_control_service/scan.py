from app.infrastructure.mqtt import topics
from app.infrastructure.protocol.packet_io import PacketBuilder
from app.infrastructure.protocol.modes import Sys
from app.infrastructure.mqtt.gmqtt_client import UnitLabMqttClient
from app.core.logger import get_logger

logger = get_logger("mqtt")


def scan_devices_now():
    topic = topics.scan()
    builder = PacketBuilder()
    builder.build(Sys.SCAN, packet_id=0, ts=0, payload=b"")
    payload = builder.to_bytes()

    logger.info(f"📤 OUT → {topic} | payload={payload.hex().upper()} (SCAN)")

    UnitLabMqttClient.get_instance().publish(topic, payload)
