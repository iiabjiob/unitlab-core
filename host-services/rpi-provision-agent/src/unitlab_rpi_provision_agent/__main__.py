from __future__ import annotations

import asyncio
import logging

from .agent import CoreProvisionAgent
from .config import load_config


def _setup_logging(level: str) -> None:
    logging.basicConfig(level=getattr(logging, level.upper(), logging.INFO), format="%(asctime)s - %(levelname)s - %(message)s")


async def _run() -> None:
    config = load_config()
    _setup_logging(config.log_level)
    agent = CoreProvisionAgent(config)
    await agent.start()
    try:
        while True:
            await asyncio.sleep(3600)
    finally:
        await agent.stop()


def main() -> None:
    asyncio.run(_run())


if __name__ == "__main__":
    main()

