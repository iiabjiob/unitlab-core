import subprocess
import platform
from app.core.logger import logger  # ✅ Import logger

class WifiService:

    @staticmethod
    def scan_wifi_linux():
        """Scans available Wi-Fi networks using nmcli (Linux only)."""
        if platform.system() != "Linux":
            logger.warning("⚠️ Wi-Fi scanning is only supported on Linux. Skipping...")
            return {"error": "System not supported. Only Linux is supported for Wi-Fi scanning."}

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
                if len(parts) >= 2:
                    ssid = parts[0].strip()
                    signal = parts[1].strip()
                    if ssid:  # Ignore empty SSIDs
                        networks.append({"ssid": ssid, "signal": signal})

            if networks:
                logger.debug(f"📡 Found Wi-Fi networks: {networks}")
                return networks
            else:
                logger.warning("⚠️ No Wi-Fi networks found.")
                return {"error": "No networks found"}

        except FileNotFoundError:
            logger.error("❌ `nmcli` command not found. Ensure NetworkManager is installed.")
        except subprocess.CalledProcessError as e:
            logger.error(f"❌ Failed to scan Wi-Fi: {e}")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error in `scan_wifi_linux`: {e}")

        return {"error": "Failed to scan Wi-Fi"}

    @staticmethod
    def connect_to_wifi(ssid: str, password: str):
        """Connects to a specified Wi-Fi network using nmcli (Linux only)."""
        if platform.system() != "Linux":
            logger.warning("⚠️ Wi-Fi connection is only supported on Linux. Skipping...")
            return {"error": "System not supported. Only Linux is supported for Wi-Fi connections."}

        try:
            logger.info(f"🔗 Connecting to Wi-Fi: {ssid}...")
            result = subprocess.run(
                ["nmcli", "dev", "wifi", "connect", ssid, "password", password],
                capture_output=True,
                text=True,
                check=True
            )

            if result.returncode == 0:
                logger.info(f"✅ Successfully connected to Wi-Fi: {ssid}")
                return {"status": "connected", "ssid": ssid}
            else:
                logger.error(f"❌ Failed to connect to Wi-Fi `{ssid}`: {result.stderr.strip()}")
                return {"error": result.stderr.strip() or "Failed to connect"}

        except FileNotFoundError:
            logger.error("❌ `nmcli` command not found. Ensure NetworkManager is installed.")
        except subprocess.CalledProcessError as e:
            logger.error(f"❌ Connection error: {e}")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error in `connect_to_wifi`: {e}")

        return {"error": "Failed to connect"}

    @staticmethod
    def get_wifi_ssid():
        """Retrieves the currently connected Wi-Fi SSID."""
        if platform.system() != "Linux":
            logger.warning("⚠️ Wi-Fi SSID retrieval is only supported on Linux. Skipping...")
            return "Not supported"

        try:
            result = subprocess.run(["nmcli", "-t", "-f", "SSID", "dev", "wifi"], capture_output=True, text=True, check=True)
            ssid = result.stdout.strip()
            logger.debug(f"📡 Current Wi-Fi SSID: {ssid if ssid else 'Not connected'}")
            return ssid if ssid else "Not connected"

        except FileNotFoundError:
            logger.error("❌ `nmcli` command not found. Cannot retrieve Wi-Fi SSID.")
        except subprocess.CalledProcessError:
            logger.error("❌ Failed to retrieve Wi-Fi SSID.")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error in `get_wifi_ssid`: {e}")

        return "Not connected"

    @staticmethod
    def get_wifi_signal():
        """Retrieves the signal strength of the connected Wi-Fi network."""
        if platform.system() != "Linux":
            logger.warning("⚠️ Wi-Fi signal strength retrieval is only supported on Linux. Skipping...")
            return "Not supported"

        try:
            result = subprocess.run(["nmcli", "-t", "-f", "SIGNAL", "dev", "wifi"], capture_output=True, text=True, check=True)
            signal = result.stdout.strip()
            logger.debug(f"📶 Wi-Fi Signal Strength: {signal}%")
            return f"{signal}%" if signal else "N/A"

        except FileNotFoundError:
            logger.error("❌ `nmcli` command not found. Cannot retrieve Wi-Fi signal strength.")
        except subprocess.CalledProcessError:
            logger.error("❌ Failed to retrieve Wi-Fi signal strength.")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error in `get_wifi_signal`: {e}")

        return "N/A"

    @staticmethod
    def get_wifi_info():
        """Retrieves the current Wi-Fi connection information."""
        if platform.system() != "Linux":
            logger.warning("⚠️ Wi-Fi info retrieval is only supported on Linux. Skipping...")
            return {"error": "Wi-Fi info not supported on this OS"}

        info = {
            "ssid": WifiService.get_wifi_ssid(),
            "signal": WifiService.get_wifi_signal(),
        }
        logger.debug(f"📊 Wi-Fi Info: {info}")
        return info
