import subprocess
import platform
from app.core.logger import logger
from app.core.utils import ensure_linux

class WifiService:

    @staticmethod
    def scan_wifi_linux():
        if not ensure_linux("Wi-Fi connection"):
            return {"error": "Only Linux is supported for Wi-Fi connections."}

        try:
            logger.info("🔍 Scanning for available Wi-Fi networks...")
            result = subprocess.run(
                ["nmcli", "-t", "-f", "SSID,SIGNAL", "dev", "wifi"],
                capture_output=True,
                text=True,
                check=True
            )

            networks = []
            for line in result.stdout.splitlines():
                parts = line.split(":")
                if len(parts) >= 2 and parts[0].strip():
                    networks.append({"ssid": parts[0].strip(), "signal": parts[1].strip()})

            if networks:
                logger.debug(f"📡 Found networks: {networks}")
                return networks

            logger.warning("⚠️ No Wi-Fi networks found.")
            return {"error": "No networks found"}

        except subprocess.CalledProcessError as e:
            logger.error(f"💥 nmcli error: {e.stderr.strip()}")
        except FileNotFoundError:
            logger.error("💥 nmcli command not found.")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error during Wi-Fi scan: {e}")

        return {"error": "Failed to scan Wi-Fi"}

    @staticmethod
    def connect_to_wifi(ssid: str, password: str):
        if not ensure_linux("Wi-Fi connection"):
            return {"error": "Only Linux is supported for Wi-Fi connections."}
        
        try:
            logger.info(f"🔗 Connecting to Wi-Fi: {ssid}...")
            subprocess.run(
                ["nmcli", "dev", "wifi", "connect", ssid, "password", password],
                capture_output=True,
                text=True,
                check=True
            )
            logger.info(f"✅ Successfully connected to Wi-Fi: {ssid}")
            return {"status": "connected", "ssid": ssid}

        except subprocess.CalledProcessError as e:
            error_msg = e.stderr.strip()
            logger.error(f"💥 Failed to connect to Wi-Fi `{ssid}`: {error_msg}")
            return {"error": error_msg}
        except FileNotFoundError:
            logger.error("💥 nmcli command not found.")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error during Wi-Fi connection: {e}")

        return {"error": "Failed to connect"}

    @staticmethod
    def get_wifi_ssid():
        if not ensure_linux("Wi-Fi connection"):
            return {"error": "Wi-Fi SSID retrieval is only supported on Linux"}

        try:
            result = subprocess.run(
                ["nmcli", "-t", "-f", "active,ssid", "dev", "wifi"],
                capture_output=True,
                text=True,
                check=True
            )
            for line in result.stdout.splitlines():
                active, ssid = line.strip().split(":", 1)
                if active == "yes":
                    logger.debug(f"📡 Current Wi-Fi SSID: {ssid}")
                    return ssid or "Not connected"
            return "Not connected"

        except subprocess.CalledProcessError as e:
            logger.error(f"💥 nmcli error: {e.stderr.strip()}")
        except FileNotFoundError:
            logger.error("💥 nmcli command not found.")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error getting SSID: {e}")

        return "Not connected"

    @staticmethod
    def get_wifi_signal():
        if not ensure_linux("Wi-Fi connection"):
            return {"error": "Wi-Fi signal strength retrieval is only supported on Linux."}

        try:
            result = subprocess.run(
                ["nmcli", "-t", "-f", "active,signal", "dev", "wifi"],
                capture_output=True,
                text=True,
                check=True
            )
            for line in result.stdout.splitlines():
                active, signal = line.strip().split(":", 1)
                if active == "yes":
                    logger.debug(f"📶 Wi-Fi Signal Strength: {signal}%")
                    return f"{signal}%"
            return "N/A"

        except subprocess.CalledProcessError as e:
            logger.error(f"💥 nmcli error: {e.stderr.strip()}")
        except FileNotFoundError:
            logger.error("💥 nmcli command not found.")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error getting signal strength: {e}")

        return "N/A"

    @staticmethod
    def get_wifi_info():
        if not ensure_linux("Wi-Fi connection"):
            return {"error": "Wi-Fi info retrieval is only supported on Linux."}

        info = {
            "ssid": WifiService.get_wifi_ssid(),
            "signal": WifiService.get_wifi_signal(),
        }
        logger.debug(f"📊 Wi-Fi Info: {info}")
        return info
