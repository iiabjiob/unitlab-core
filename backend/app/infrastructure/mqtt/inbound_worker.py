import time

from app.core.logger import get_logger
from app.core.mqtt_dto import InboundMqttMsg
from app.infrastructure.mqtt.router import MqttRouter

logger = get_logger("inbo")
router = MqttRouter()


async def process_inbound_message(msg: InboundMqttMsg) -> None:
    """Route and handle a single inbound MQTT message."""

    try:
        queue_latency = max((time.time() * 1000) - (msg.ts_ms or 0), 0)

        start_route = time.perf_counter()
        handler, pattern = router.find_handler(msg.topic)
        unit_id = router.extract_unit_id(msg.topic)
        route_latency = (time.perf_counter() - start_route) * 1000

        start_handler = time.perf_counter()
        if handler:
            await handler(msg.topic, msg.payload, unit_id)
        else:
            logger.warning(f"[INBO] No handler found for {msg.topic}")
        handler_latency = (time.perf_counter() - start_handler) * 1000

        logger.debug(
            f"[INBO] {msg.topic} ({pattern}) | "
            f"queue={queue_latency:.1f} ms, "
            f"route={route_latency:.1f} ms, "
            f"handler={handler_latency:.1f} ms"
        )
    except Exception as exc:
        logger.error(f"[INBO] 💥 Error while handling {msg.topic}: {exc}")
