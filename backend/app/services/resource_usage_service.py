import psutil
from app.core.logger import logger

class ResourceUsageService:
    """ Retrieves CPU, RAM, and Disk usage metrics """

    @staticmethod
    def get_cpu_usage():
        """Returns the current CPU usage percentage"""
        try:
            cpu_usage = psutil.cpu_percent(interval=1)
            logger.debug(f"🖥️ CPU Usage: {cpu_usage}%")
            return f"{cpu_usage}%"
        except Exception as e:
            logger.exception(f"⚠️ Failed to get CPU usage: {e}")
            return "Error retrieving CPU usage"

    @staticmethod
    def get_ram_usage():
        """Returns RAM usage details in GB"""
        try:
            ram = psutil.virtual_memory()
            usage = {
                "total": round(ram.total / (1024 ** 3), 2),
                "used": round(ram.used / (1024 ** 3), 2),
                "available": round(ram.available / (1024 ** 3), 2),
            }
            logger.debug(f"💾 RAM Usage: {usage}")
            return usage
        except Exception as e:
            logger.exception(f"⚠️ Failed to get RAM usage: {e}")
            return {"error": "Error retrieving RAM usage"}

    @staticmethod
    def get_disk_usage():
        """Returns Disk usage details in GB"""
        try:
            disk = psutil.disk_usage("/")
            usage = {
                "total": round(disk.total / (1024 ** 3), 2),
                "used": round(disk.used / (1024 ** 3), 2),
                "free": round(disk.free / (1024 ** 3), 2),
            }
            logger.debug(f"📀 Disk Usage: {usage}")
            return usage
        except Exception as e:
            logger.exception(f"⚠️ Failed to get Disk usage: {e}")
            return {"error": "Error retrieving Disk usage"}
