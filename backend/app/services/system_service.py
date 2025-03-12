import platform
import socket
import psutil
import subprocess

class SystemService:

    @staticmethod
    def get_system_info():
        try:
            # Основные данные
            hostname = socket.gethostname()
            ip_address = socket.gethostbyname(hostname)
            os_name = platform.system()
            os_version = platform.version()
            uptime_seconds = int(psutil.boot_time())

            # Температура (доступно только на Linux)
            temp = None
            if os_name == "Linux":
                try:
                    result = subprocess.run(["cat", "/sys/class/thermal/thermal_zone0/temp"], capture_output=True, text=True)
                    temp = round(int(result.stdout.strip()) / 1000, 1)  # Преобразуем в градусы
                except Exception:
                    temp = "Not available"

            # Загрузка процессора и RAM
            cpu_usage = psutil.cpu_percent(interval=1)
            ram = psutil.virtual_memory()
            disk = psutil.disk_usage("/")

            # Сетевые данные
            wifi_ssid = None
            wifi_signal = None
            try:
                result = subprocess.run(["nmcli", "-t", "-f", "SSID,SIGNAL", "dev", "wifi"], capture_output=True, text=True)
                lines = result.stdout.splitlines()
                if lines:
                    wifi_ssid, wifi_signal = lines[0].split(":")
            except Exception:
                pass

            return {
                "hostname": hostname,
                "ip_address": ip_address,
                "os": f"{os_name} {os_version}",
                "uptime": uptime_seconds,
                "cpu_usage": f"{cpu_usage}%",
                "temperature": f"{temp}°C" if temp else "N/A",
                "ram": {
                    "total": round(ram.total / (1024 ** 3), 2),  # в ГБ
                    "used": round(ram.used / (1024 ** 3), 2),
                    "available": round(ram.available / (1024 ** 3), 2),
                },
                "disk": {
                    "total": round(disk.total / (1024 ** 3), 2),
                    "used": round(disk.used / (1024 ** 3), 2),
                    "free": round(disk.free / (1024 ** 3), 2),
                },
                "wifi": {
                    "ssid": wifi_ssid if wifi_ssid else "Not connected",
                    "signal": f"{wifi_signal}%" if wifi_signal else "N/A"
                }
            }

        except Exception as e:
            return {"error": str(e)}