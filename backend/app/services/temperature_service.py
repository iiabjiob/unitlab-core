import platform
import subprocess

class TemperatureService:
    """ Получает температуру процессора (Linux only) """

    @staticmethod
    def get_temperature():
        if platform.system() == "Linux":
            try:
                result = subprocess.run(["cat", "/sys/class/thermal/thermal_zone0/temp"], capture_output=True, text=True)
                return f"{round(int(result.stdout.strip()) / 1000, 1)}°C"
            except Exception:
                return "Not available"
        return "N/A"