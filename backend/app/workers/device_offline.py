from __future__ import annotations

import asyncio
import signal
from contextlib import suppress

from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.services.worker_health import clear_worker_status, start_worker_heartbeat
from app.tasks.device_offline_task import device_offline_checker

logger = get_logger("worker.offline")


async def main() -> None:
    await RedisManager.start()

    stop_event = asyncio.Event()
    heartbeat_task = start_worker_heartbeat("device_offline")

    def _signal_handler() -> None:
        logger.info("🛑 Stop signal received, shutting down offline checker...")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, _signal_handler)
        except NotImplementedError:
            pass

    checker_task = asyncio.create_task(device_offline_checker())
    logger.info("🚀 Offline checker worker started")

    try:
        await stop_event.wait()
    finally:
        checker_task.cancel()
        with suppress(asyncio.CancelledError):
            await checker_task
        heartbeat_task.cancel()
        with suppress(asyncio.CancelledError):
            await heartbeat_task
        await clear_worker_status("device_offline")
        await RedisManager.stop()
        logger.info("✅ Offline checker worker stopped")


if __name__ == "__main__":
    asyncio.run(main())
