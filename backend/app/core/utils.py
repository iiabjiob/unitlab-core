# app/core/system_utils.py
import platform
import subprocess
from app.core.logger import get_logger
from app.core.config import get_settings

logger = get_logger("core")
settings = get_settings()

def is_linux():
    return platform.system() == "Linux"

def ensure_linux(feature_name=""):
    if not is_linux():
        msg = f"⚠️ {feature_name} is only supported on Linux."
        logger.warning(msg)
        return False
    return True

def enable_ap_mode():
    if not ensure_linux("Access Point Mode"):
        return
    
    suffix = get_mac_suffix()
    ssid = f"{settings.ap_ssid_prefix}{suffix}"
    password = f"{settings.ap_password_prefix}{suffix}"

    try:
        logger.info(f"🔧 Setting up Access Point: SSID={ssid}")
        
        # Сначала убедимся, что NetworkManager активен
        subprocess.run(["nmcli", "radio", "wifi", "on"], check=True)

        # Удалим возможный конфликтующий hotspot
        subprocess.run(["nmcli", "connection", "delete", "Hotspot"], check=False)

        # Создадим новый hotspot
        subprocess.run([
            "nmcli", "device", "wifi", "hotspot",
            "ifname", "wlan0",
            "con-name", "Hotspot",
            "ssid", ssid,
            "password", password
        ], check=True)

        logger.info("✅ Access Point enabled.")
    except subprocess.CalledProcessError as e:
        logger.error(f"❌ Failed to enable Access Point: {e}")

def get_mac_suffix(interface: str = "wlan0") -> str:
    try:
        mac = open(f"/sys/class/net/{interface}/address").read().strip()
        # Возьмём последние 2 байта MAC без двоеточий (например, ABCD)
        parts = mac.split(":")[-2:]
        return ''.join(part.upper() for part in parts)
    except Exception as e:
        logger.warning(f"⚠️ Failed to read MAC address: {e}")
        return "XXXX"