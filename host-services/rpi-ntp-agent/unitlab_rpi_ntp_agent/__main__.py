from __future__ import annotations

import asyncio
import logging
import signal

from .agent import CoreNtpAgent
from .config import load_config


def _configure_logging(level: str) -> None:
    logging.basicConfig(
        level=getattr(logging, level.upper(), logging.INFO),
        format="%(asctime)s %(levelname)s [%(name)s] %(message)s",
    )


async def _run() -> None:
    config = load_config()
    _configure_logging(config.log_level)
    agent = CoreNtpAgent(config)
    stop_event = asyncio.Event()

    loop = asyncio.get_running_loop()

    def _request_stop() -> None:
        if not stop_event.is_set():
            logging.getLogger("unitlab.ntp_agent").info("Shutdown signal received")
            stop_event.set()

    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, _request_stop)
        except NotImplementedError:
            pass

    await agent.start()
    try:
        await stop_event.wait()
    finally:
        await agent.stop()


def main() -> None:
    asyncio.run(_run())


if __name__ == "__main__":
    main()

