import time

from app.core.logger import get_logger
from app.core.mqtt_dto import OutboundCmdMsg
from app.infrastructure.mqtt.gmqtt_client import UnitLabMqttClient
from app.infrastructure.db.database import AsyncSessionLocal
from app.services.hardware_command_intent import hardware_command_publish_allowed

logger = get_logger("outb")


async def publish_outbound_message(mqtt: UnitLabMqttClient, msg: OutboundCmdMsg) -> None:
    """Publish a single command via MQTT and log latency metrics."""

    try:
        if msg.command_id:
            async with AsyncSessionLocal() as session:
                allowed = await hardware_command_publish_allowed(session, command_id=msg.command_id)
            if not allowed:
                logger.warning(
                    "Discarding expired or terminal hardware command: command_id=%s packet_id=%s topic=%s",
                    msg.command_id, msg.packet_id, msg.topic,
                )
                return
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
