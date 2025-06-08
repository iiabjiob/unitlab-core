# app/core/utils.py
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
        subprocess.run(["sudo", "nmcli", "radio", "wifi", "on"], check=True)

        # Удалим возможный конфликтующий hotspot
        subprocess.run(["sudo", "nmcli", "connection", "delete", "Hotspot"], check=False)

        # Создадим новый hotspot
        subprocess.run([
            "sudo", "nmcli", "device", "wifi", "hotspot",
            "ifname", "wlan0",
            "con-name", "Hotspot",
            "ssid", ssid,
            "password", password
        ], check=True)

        logger.info("✅ Access Point enabled.")
    except subprocess.CalledProcessError as e:
        logger.error(f"❌ Failed to enable Access Point: {e}")

def get_mac_suffix(interface: str = "wlan0", bytes_count: int = 2) -> str:
    """
    Returns MAC address suffix for device naming.
    :param interface: network interface (default 'wlan0')
    :param bytes_count: how many last bytes to use (default 2)
    :return: MAC suffix (e.g. 'ABCD')
    """
    try:
        with open(f"/sys/class/net/{interface}/address") as f:
            mac = f.read().strip()
        parts = mac.split(":")[-bytes_count:]
        return ''.join(part.upper() for part in parts)
    except Exception as e:
        logger.warning(f"⚠️ Failed to read MAC address from {interface}: {e}")
        return 'X' * (bytes_count * 2)


def get_unit_id(interface: str = "wlan0", bytes_count: int = 2) -> str:
    """
    Returns unitId for this device, e.g., coreUnit-ABCD.
    """
    suffix = get_mac_suffix(interface, bytes_count)
    return f"coreUnit-{suffix}"