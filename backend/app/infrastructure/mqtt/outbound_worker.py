# outbound_publisher_worker.py
import time
from app.core.logger import get_logger
from app.core.message_bus import MessageBus, OutboundCmdMsg
from app.infrastructure.mqtt.manager import MqttManager

logger = get_logger("outb")

async def run_outbound_publisher_worker():
    bus = MessageBus.get_instance()
    mqtt = MqttManager.get_instance()

    while True:
        msg: OutboundCmdMsg = await bus.outbound_cmd_q.get()
        try:
            # 1) queue latency
            queue_latency = (time.time() * 1000) - msg.enqueued_at_ms

            # 2) publish latency
            start_pub = time.perf_counter()
            mqtt.publish(msg.topic, msg.payload, qos=msg.qos, retain=msg.retain)
            pub_latency = (time.perf_counter() - start_pub) * 1000

            logger.info(
                f"[OUTB] 📤 OUT → {msg.topic} "
                f"(size={len(msg.payload)} bytes, pid={msg.packet_id}, corr={msg.correlation_id}) | "
                f"queue={queue_latency:.1f} ms, publish={pub_latency:.1f} ms"
            )

        except Exception as e:
            logger.error(f"[OUTB] 💥 Error while publishing to {msg.topic}: {e}")
        finally:
            bus.outbound_cmd_q.task_done()
