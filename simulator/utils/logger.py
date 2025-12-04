"""Logging helpers for the UnitLab simulator.

The simulator runs thousands of concurrent asyncio tasks, so this module
centralizes logging configuration and naming conventions. It keeps the surface
minimal to avoid leaking the logging setup across other modules.
"""

from __future__ import annotations

import logging
from typing import Optional

_DEFAULT_FORMAT = "%(asctime)s [%(levelname)s] %(name)s: %(message)s"


def setup_logging(level: int = logging.INFO) -> None:
    """Configure the root logger once using a thread-safe guard."""
    root = logging.getLogger()
    if root.handlers:
        root.setLevel(level)
        return
    handler = logging.StreamHandler()
    handler.setFormatter(logging.Formatter(_DEFAULT_FORMAT))
    root.addHandler(handler)
    root.setLevel(level)


def get_logger(name: str, *, level: Optional[int] = None) -> logging.Logger:
    """Return a child logger scoped under the simulator namespace."""
    logger = logging.getLogger(f"simulator.{name}")
    if level is not None:
        logger.setLevel(level)
    return logger
