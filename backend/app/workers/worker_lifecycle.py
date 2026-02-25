from __future__ import annotations

import asyncio
import signal
from typing import Any, Awaitable, Callable


def install_stop_signal_handlers(
    *,
    stop_event: asyncio.Event,
    logger: Any,
    stop_message: str,
) -> None:
    def _signal_handler() -> None:
        logger.info(stop_message)
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(sig, _signal_handler)
        except NotImplementedError:
            # Windows / limited runtimes may not support signal handlers on this loop.
            pass


async def run_consume_loop(
    *,
    stop_event: asyncio.Event,
    fetch_entries: Callable[[], Awaitable[list[Any]]],
    process_entries: Callable[[list[Any]], Awaitable[None]],
    logger: Any | None = None,
    retry_delay_sec: float = 0.5,
) -> None:
    while not stop_event.is_set():
        try:
            entries = await fetch_entries()
        except asyncio.CancelledError:
            raise
        except Exception as exc:  # noqa: BLE001
            if stop_event.is_set():
                if logger is not None:
                    logger.info("Consume loop stopped after shutdown signal (%s)", exc)
                break
            if logger is not None:
                logger.warning("Consume loop fetch failed, retrying in %.2fs: %s", retry_delay_sec, exc)
            await asyncio.sleep(max(0.05, float(retry_delay_sec)))
            continue
        if not entries:
            continue
        await process_entries(entries)
