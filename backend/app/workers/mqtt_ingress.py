from __future__ import annotations

import asyncio
import signal

from app.core.logger import get_logger
from app.core.mqtt_dto import InboundMqttMsg
from app.infrastructure.mqtt.manager import MqttManager
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import enqueue_inbound_message

logger = get_logger("worker.ingress")


async def _handle_incoming(topic: str, payload: bytes, qos: int, properties: object):
    msg = InboundMqttMsg(topic=topic, payload=payload, qos=qos)
    try:
        await enqueue_inbound_message(msg)
        logger.debug("[Ingress] Stored %s (%d bytes)", topic, len(payload))
    except Exception as exc:
        logger.error("💥 Failed to enqueue inbound MQTT message %s: %s", topic, exc)
        raise


async def main() -> None:
    await RedisManager.start()
    await MqttManager.start(client_id="unitlab-ingress", on_message=_handle_incoming)

    stop_event = asyncio.Event()

    def _signal_handler() -> None:
        logger.info("🛑 Stop signal received, shutting down MQTT ingress worker...")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(sig, _signal_handler)
        except NotImplementedError:
            # Windows fallback
            pass

    await stop_event.wait()

    await MqttManager.stop()
    await RedisManager.stop()
    logger.info("✅ MQTT ingress worker stopped")


if __name__ == "__main__":
    asyncio.run(main())
