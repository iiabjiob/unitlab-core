# app/core/system_utils.py
import platform
from app.core.logger import logger

def is_linux():
    return platform.system() == "Linux"

def ensure_linux(feature_name=""):
    if not is_linux():
        msg = f"⚠️ {feature_name} is only supported on Linux."
        logger.warning(msg)
        return False
    return True
