from __future__ import annotations

import asyncio
import os
import signal
import socket
from contextlib import suppress
from typing import List, Tuple

from redis.exceptions import ConnectionError as RedisConnectionError, ResponseError

from app.core.config import get_settings
from app.core.logger import get_logger
from app.core.mqtt_dto import InboundMqttMsg
from app.infrastructure.mqtt.handlers import bootstrap  # noqa: F401
from app.infrastructure.mqtt.inbound_worker import process_inbound_message
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import parse_inbound_entry
from app.services.worker_health import clear_worker_status, start_worker_heartbeat

settings = get_settings()
logger = get_logger("worker.inbound")

STREAM_NAME = settings.mqtt_in_stream
GROUP_NAME = "mqtt-inbound"
CONSUMER_NAME = f"{socket.gethostname()}-{os.getpid()}"

StreamEntries = List[Tuple[str, dict]]


async def _ensure_group(redis) -> None:
    try:
        await redis.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        logger.info("✅ Created consumer group %s for stream %s", GROUP_NAME, STREAM_NAME)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            logger.info("ℹ️ Consumer group %s already exists", GROUP_NAME)
        else:
            raise


async def _process_entries(redis, entries: StreamEntries) -> None:
    for entry_id, fields in entries:
        try:
            entry_id, msg = parse_inbound_entry((entry_id, fields))
            await process_inbound_message(msg)
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
        except Exception as exc:
            logger.error("💥 Failed to process inbound entry %s: %s", entry_id, exc)


async def _fetch(redis, stream_id: str, block_ms: int = 5000) -> StreamEntries:
    result = await redis.xreadgroup(
        GROUP_NAME,
        CONSUMER_NAME,
        streams={STREAM_NAME: stream_id},
        count=20,
        block=block_ms,
    )
    if not result:
        return []
    _, entries = result[0]
    return entries


async def _drain_pending(redis) -> None:
    while True:
        entries = await _fetch(redis, "0", block_ms=100)
        if not entries:
            break
        logger.info("🔁 Replaying %d pending entries", len(entries))
        await _process_entries(redis, entries)


async def main() -> None:
    await RedisManager.start()
    redis = RedisManager.get_instance()

    await _ensure_group(redis)
    await _drain_pending(redis)
    heartbeat_task = start_worker_heartbeat("inbound_processor")

    logger.info(
        "🚀 Inbound processor ready (stream=%s, group=%s, consumer=%s)",
        STREAM_NAME,
        GROUP_NAME,
        CONSUMER_NAME,
    )

    stop_event = asyncio.Event()

    def _signal_handler() -> None:
        logger.info("🛑 Stop signal received, shutting down inbound processor...")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(sig, _signal_handler)
        except NotImplementedError:
            pass

    try:
        while not stop_event.is_set():
            try:
                entries = await _fetch(redis, ">")
            except RedisConnectionError as exc:
                if stop_event.is_set():
                    logger.info("Inbound processor stopping after Redis disconnect: %s", exc)
                    break
                logger.warning("Inbound processor Redis fetch failed, retrying: %s", exc)
                await asyncio.sleep(0.5)
                continue
            if not entries:
                continue
            await _process_entries(redis, entries)
    finally:
        heartbeat_task.cancel()
        with suppress(asyncio.CancelledError):
            await heartbeat_task
        await clear_worker_status("inbound_processor")
        await RedisManager.stop()


if __name__ == "__main__":
    asyncio.run(main())
