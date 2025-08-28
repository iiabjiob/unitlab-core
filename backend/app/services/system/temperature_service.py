import platform
import subprocess
from app.core.logger import logger
from app.core.utils import ensure_linux

class TemperatureService:
    """ Retrieves CPU temperature (Linux only) """

    @staticmethod
    def get_temperature():
        """Returns the CPU temperature in °C (Linux only)"""

        if not ensure_linux("CPU temperature"):
            return "N/A"

        try:
            result = subprocess.run(["cat", "/sys/class/thermal/thermal_zone0/temp"], capture_output=True, text=True, check=True)
            temperature = round(int(result.stdout.strip()) / 1000, 1)
            logger.debug(f"🌡️ CPU Temperature: {temperature}°C")
            return f"{temperature}°C"

        except FileNotFoundError:
            logger.error("❌ Temperature file `/sys/class/thermal/thermal_zone0/temp` not found. CPU temperature unavailable.")
        except PermissionError:
            logger.error("❌ Permission denied when accessing `/sys/class/thermal/thermal_zone0/temp`. Try running as root.")
        except ValueError:
            logger.error(f"⚠️ Invalid data in temperature file: {result.stdout.strip()}")
        except subprocess.CalledProcessError as e:
            logger.error(f"❌ Failed to execute command: {e}")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error in `get_temperature`: {e}")

        return "Not available"
