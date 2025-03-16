import platform
import socket
import psutil
import datetime
from urllib.parse import urlparse
from app.core.logger import logger
from app.core.config import get_settings

settings = get_settings()

class SystemInfoService:
    """ Retrieves system information """

    @staticmethod
    def get_app_version():
        """ Returns the app version """
        return settings.version

    @staticmethod
    def get_hostname():
        """ Returns the system hostname """
        hostname = socket.gethostname()
        logger.debug(f"🖥️ Hostname: {hostname}")
        return hostname

    @staticmethod
    def get_ip_address():
        parsed_url = urlparse(settings.base_url)  # ✅ Разбираем URL
        return parsed_url.hostname  # ✅ Возвращаем только hostname

    @staticmethod
    def get_os():
        """ Returns the OS name and version """
        os_info = f"{platform.system()} {platform.version()}"
        logger.debug(f"🖥️ OS: {os_info}")
        return os_info

    @staticmethod
    def get_uptime():
        """ Returns system uptime in format DD:HH:MM:SS """
        try:
            uptime_seconds = int(psutil.boot_time())
            uptime_timedelta = datetime.datetime.now() - datetime.datetime.fromtimestamp(uptime_seconds)

            days = uptime_timedelta.days
            hours, remainder = divmod(uptime_timedelta.seconds, 3600)
            minutes, seconds = divmod(remainder, 60)

            uptime_str = f"{days}d {hours}h {minutes}m"
            logger.debug(f"⏳ System Uptime: {uptime_str}")
            return uptime_str

        except Exception as e:
            logger.exception(f"❌ Failed to get system uptime: {e}")
            return "Error retrieving uptime"
