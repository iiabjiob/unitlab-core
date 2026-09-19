"""Small cross-platform helpers used by hardware-adjacent services."""
import sys

from app.core.logger import get_logger
from app.core.config import get_settings

logger = get_logger("core")
settings = get_settings()


def ensure_linux(feature: str) -> bool:
    """Return whether a Linux-only host feature can be queried."""
    if sys.platform != "linux":
        logger.debug("%s is available only on Linux (platform=%s)", feature, sys.platform)
        return False
    return True

def to_str(val: object, default: str | None = None) -> str | None:
    if isinstance(val, (bytes, bytearray)):
        return val.decode()
    if isinstance(val, str):
        return val
    return default

def to_int(val: object, default: int | None = None) -> int | None:
    s = to_str(val)
    if s and s.isdigit():
        return int(s)
    return default
