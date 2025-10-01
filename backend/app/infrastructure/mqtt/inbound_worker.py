# inbound_router_worker.py
import time
from app.core.logger import get_logger
from app.infrastructure.mqtt.router import MqttRouter
from app.core.message_bus import MessageBus, InboundMqttMsg

logger = get_logger("inbo")

async def run_inbound_router_worker():
    bus = MessageBus.get_instance()
    router = MqttRouter()

    while True:
        msg: InboundMqttMsg = await bus.inbound_mqtt_q.get()
        try:
            # 1) queue latency (сколько пролежало в очереди)
            queue_latency = (time.time() * 1000) - msg.ts_ms

            # 2) routing latency (regex match)
            start_route = time.perf_counter()
            
            handler, pattern = router.find_handler(msg.topic)
            unit_id = router.extract_unit_id(msg.topic)

            route_latency = (time.perf_counter() - start_route) * 1000

            # 3) handler latency
            start_handler = time.perf_counter()
            if handler:
                await handler(msg.topic, msg.payload, unit_id)
            else:
                logger.warning(f"[INBO] No handler found for {msg.topic}")
            handler_latency = (time.perf_counter() - start_handler) * 1000

            logger.debug(
                f"[INBO] {msg.topic} | "
                f"queue={queue_latency:.1f} ms, "
                f"route={route_latency:.1f} ms, "
                f"handler={handler_latency:.1f} ms"
            )

        except Exception as e:
            logger.error(f"[INBO] 💥 Error while handling {msg.topic}: {e}")
        finally:
            bus.inbound_mqtt_q.task_done()
