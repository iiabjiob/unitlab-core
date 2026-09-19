from importlib import import_module
from typing import Protocol, cast

from app.core.logger import logger


class _PsutilMemoryInfo(Protocol):
    total: int
    used: int
    available: int


class _PsutilDiskInfo(Protocol):
    total: int
    used: int
    free: int


class _Psutil(Protocol):
    def cpu_percent(self, *, interval: float | None = None) -> float: ...

    def virtual_memory(self) -> _PsutilMemoryInfo: ...

    def disk_usage(self, path: str) -> _PsutilDiskInfo: ...

try:
    psutil: _Psutil | None = cast(_Psutil, cast(object, import_module("psutil")))
except ImportError:  # pragma: no cover - optional host metric dependency
    psutil = None
class ResourceUsageService:
    """ Retrieves CPU, RAM, and Disk usage metrics """

    @staticmethod
    def get_cpu_usage() -> str:
        """Returns the current CPU usage percentage"""
        try:
            if psutil is None:
                return "Unavailable"
            cpu_usage = psutil.cpu_percent(interval=None)
            logger.debug(f"🖥️ CPU Usage: {cpu_usage}%")
            return f"{cpu_usage}%"
        except Exception as e:
            logger.exception(f"⚠️ Failed to get CPU usage: {e}")
            return "Error retrieving CPU usage"

    @staticmethod
    def get_ram_usage() -> dict[str, float] | dict[str, str]:
        """Returns RAM usage details in GB"""
        try:
            if psutil is None:
                return {"error": "psutil is not installed"}
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
    def get_disk_usage() -> dict[str, float] | dict[str, str]:
        """Returns Disk usage details in GB"""
        try:
            if psutil is None:
                return {"error": "psutil is not installed"}
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
