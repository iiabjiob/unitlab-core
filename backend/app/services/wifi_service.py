import subprocess
import platform

class WifiService:

    @staticmethod
    def scan_wifi_linux():
        """Сканирует доступные Wi-Fi сети с помощью nmcli."""
        if platform.system() != "Linux":
            return {"error": "System not supported. Only Linux is supported for Wi-Fi scanning."}
        try:
            result = subprocess.run(
                ["nmcli", "-t", "-f", "SSID,SIGNAL", "dev", "wifi"],
                capture_output=True,
                text=True
            )
            
            networks = []
            for line in result.stdout.splitlines():
                parts = line.split(":")
                if len(parts) >= 2:
                    ssid = parts[0].strip()
                    signal = parts[1].strip()

                    # Фильтруем пустые SSID
                    if ssid:
                        networks.append({"ssid": ssid, "signal": signal})

            return networks if networks else {"error": "No networks found"}

        except Exception as e:
            return {"error": str(e)}
    
    @staticmethod
    def connect_to_wifi(ssid: str, password: str):
        """Подключается к указанной Wi-Fi сети с помощью nmcli."""
        if platform.system() != "Linux":
            return {"error": "System not supported. Only Linux is supported for Wi-Fi connections."}
        try:
            result = subprocess.run(
                ["nmcli", "dev", "wifi", "connect", ssid, "password", password],
                capture_output=True,
                text=True
            )

            if result.returncode == 0:
                return {"status": "connected", "ssid": ssid}
            else:
                return {"error": result.stderr.strip() or "Failed to connect"}

        except Exception as e:
            return {"error": str(e)}
        
    @staticmethod
    def get_wifi_ssid():
        try:
            result = subprocess.run(["nmcli", "-t", "-f", "SSID", "dev", "wifi"], capture_output=True, text=True)
            return result.stdout.strip() if result.stdout.strip() else "Not connected"
        except Exception:
            return "Not connected"

    @staticmethod
    def get_wifi_signal():
        try:
            result = subprocess.run(["nmcli", "-t", "-f", "SIGNAL", "dev", "wifi"], capture_output=True, text=True)
            return f"{result.stdout.strip()}%" if result.stdout.strip() else "N/A"
        except Exception:
            return "N/A"
    
    @staticmethod
    def get_wifi_info():
        return {
            "ssid": WifiService.get_wifi_ssid(),
            "signal": WifiService.get_wifi_signal(),
        }