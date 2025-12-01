# app/core/utils.py
from app.core.logger import get_logger
from app.core.config import get_settings

logger = get_logger("core")
settings = get_settings()

def to_str(val, default=None) -> str | None:
    if isinstance(val, (bytes, bytearray)):
        return val.decode()
    if isinstance(val, str):
        return val
    return default

def to_int(val, default=None) -> int | None:
    s = to_str(val)
    if s and s.isdigit():
        return int(s)
    return default