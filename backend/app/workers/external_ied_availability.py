from __future__ import annotations

import asyncio
import signal
from contextlib import suppress

from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.services.external_ied_availability import run_external_ied_availability_checker
from app.services.worker_health import clear_worker_status, start_worker_heartbeat

logger = get_logger("worker.external_ied")


async def main() -> None:
    await RedisManager.start()

    stop_event = asyncio.Event()
    heartbeat_task = start_worker_heartbeat("external_ied_availability")

    def _signal_handler() -> None:
        logger.info("Stop signal received, shutting down external IED availability watcher...")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, _signal_handler)
        except NotImplementedError:
            pass

    checker_task = asyncio.create_task(run_external_ied_availability_checker(stop_event))
    logger.info("External IED availability watcher started")

    try:
        await stop_event.wait()
    finally:
        checker_task.cancel()
        with suppress(asyncio.CancelledError):
            await checker_task
        heartbeat_task.cancel()
        with suppress(asyncio.CancelledError):
            await heartbeat_task
        await clear_worker_status("external_ied_availability")
        await RedisManager.stop()
        logger.info("External IED availability watcher stopped")


if __name__ == "__main__":
    asyncio.run(main())
