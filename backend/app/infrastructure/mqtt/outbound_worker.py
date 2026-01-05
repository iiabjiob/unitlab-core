import time

from app.core.logger import get_logger
from app.core.mqtt_dto import OutboundCmdMsg
from app.infrastructure.mqtt.gmqtt_client import UnitLabMqttClient

logger = get_logger("outb")


async def publish_outbound_message(mqtt: UnitLabMqttClient, msg: OutboundCmdMsg) -> None:
    """Publish a single command via MQTT and log latency metrics."""

    try:
        queue_latency = max((time.time() * 1000) - msg.enqueued_at_ms, 0)

        start_pub = time.perf_counter()
        mqtt.publish(msg.topic, msg.payload, qos=msg.qos, retain=msg.retain)
        pub_latency = (time.perf_counter() - start_pub) * 1000

        logger.info(
            f"[OUTB] 📤 OUT → {msg.topic} "
            f"(size={len(msg.payload)} bytes, pid={msg.packet_id}, corr={msg.correlation_id}) | "
            f"queue={queue_latency:.1f} ms, publish={pub_latency:.1f} ms"
        )
    except Exception as exc:
        logger.error(f"[OUTB] 💥 Error while publishing to {msg.topic}: {exc}")
        raise
