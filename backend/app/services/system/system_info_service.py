import platform
import socket
import psutil
import netifaces
import datetime
from app.core.logger import logger
from app.core.config import get_settings

settings = get_settings()

class SystemInfoService:
    """ Retrieves system information """

    @staticmethod
    def get_app_version():
        """ Returns the app version """
        return settings.app_version

    @staticmethod
    def get_hostname():
        """ Returns the system hostname """
        hostname = socket.gethostname()
        logger.debug(f"🖥️ Hostname: {hostname}")
        return hostname

    @staticmethod
    def get_ip_address():
        """Automatically retrieves the IP address from available interfaces"""
        interfaces = ['eth0', 'wlan0']
        for iface in interfaces:
            try:
                addresses = netifaces.ifaddresses(iface)
                ip_info = addresses.get(netifaces.AF_INET)
                if ip_info:
                    ip_address = ip_info[0]['addr']
                    logger.debug(f"🌐 IP Address [{iface}]: {ip_address}")
                    return ip_address
            except Exception as e:
                logger.warning(f"⚠️ Unable to get IP for '{iface}': {e}")

        logger.error("❌ No IP address found on available interfaces.")
        return "No IP assigned"

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
