from __future__ import annotations

import asyncio
import os
import signal
import socket
from contextlib import suppress

from redis.exceptions import ResponseError

from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.mqtt.manager import MqttManager
from app.infrastructure.mqtt.outbound_worker import publish_outbound_message
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import parse_outbound_entry
from app.services.worker_health import clear_worker_status, start_worker_heartbeat

settings = get_settings()
logger = get_logger("worker.outbound")

STREAM_NAME = settings.mqtt_out_stream
GROUP_NAME = "mqtt-outbound"
CONSUMER_NAME = f"{socket.gethostname()}-{os.getpid()}"


async def _ensure_group(redis) -> None:
    try:
        await redis.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        logger.info("✅ Created outbound consumer group %s", GROUP_NAME)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            logger.info("ℹ️ Outbound consumer group already exists")
        else:
            raise


async def _fetch(redis, stream_id: str, block_ms: int = 5000):
    result = await redis.xreadgroup(
        GROUP_NAME,
        CONSUMER_NAME,
        streams={STREAM_NAME: stream_id},
        count=20,
        block=block_ms,
    )
    if not result:
        return []
    return result[0][1]


async def _process_entries(redis, mqtt_client, entries) -> None:
    for entry_id, fields in entries:
        entry_id, msg = parse_outbound_entry((entry_id, fields))
        try:
            await publish_outbound_message(mqtt_client, msg)
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
        except Exception as exc:
            logger.error("💥 Failed to publish outbound message %s: %s", entry_id, exc)


async def _drain_pending(redis, mqtt_client) -> None:
    while True:
        entries = await _fetch(redis, "0", block_ms=100)
        if not entries:
            break
        logger.info("🔁 Replaying %d pending outbound entries", len(entries))
        await _process_entries(redis, mqtt_client, entries)


async def main() -> None:
    await RedisManager.start()
    redis = RedisManager.get_instance()

    await _ensure_group(redis)

    mqtt_client = await MqttManager.start(
        client_id="unitlab-outbound",
        subscriptions=None,
    )

    await _drain_pending(redis, mqtt_client)
    heartbeat_task = start_worker_heartbeat("mqtt_outbound")

    stop_event = asyncio.Event()

    def _signal_handler() -> None:
        logger.info("🛑 Stop signal received, shutting down outbound worker...")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(sig, _signal_handler)
        except NotImplementedError:
            pass

    logger.info(
        "🚀 Outbound worker ready (stream=%s, group=%s, consumer=%s)",
        STREAM_NAME,
        GROUP_NAME,
        CONSUMER_NAME,
    )

    try:
        while not stop_event.is_set():
            entries = await _fetch(redis, ">")
            if not entries:
                continue
            await _process_entries(redis, mqtt_client, entries)
    finally:
        heartbeat_task.cancel()
        with suppress(asyncio.CancelledError):
            await heartbeat_task
        await clear_worker_status("mqtt_outbound")
        await MqttManager.stop()
        await RedisManager.stop()


if __name__ == "__main__":
    asyncio.run(main())
