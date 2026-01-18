import platform
import socket
import psutil
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
    def get_active_interfaces():
        """Returns a list of active (UP) network interfaces, excluding loopback."""
        active = []
        stats = psutil.net_if_stats()
        for iface, info in stats.items():
            if info.isup and iface != 'lo':
                active.append(iface)
        logger.debug(f"🔌 Active interfaces: {active}")
        return active

    @staticmethod
    def get_ip_addresses(interfaces=None):
        """Returns IP addresses for given or active interfaces."""
        ip_addresses = {}
        all_addrs = psutil.net_if_addrs()

        if interfaces is None:
            interfaces = SystemInfoService.get_active_interfaces()

        for iface in interfaces:
            iface_addrs = all_addrs.get(iface)
            if iface_addrs:
                ipv4 = next((addr.address for addr in iface_addrs if addr.family == socket.AF_INET), None)
                if ipv4:
                    ip_addresses[iface] = ipv4
                    logger.debug(f"📡 {iface}: {ipv4}")
                else:
                    ip_addresses[iface] = "No IP assigned"
            else:
                ip_addresses[iface] = "Interface not found"

        return ip_addresses
    
    @staticmethod
    def get_local_ip_from_active_interface() -> str:
        """Return the IP address of the first active interface with a valid IPv4."""
        ip_addresses = SystemInfoService.get_ip_addresses()
        for iface, ip in ip_addresses.items():
            if SystemInfoService._is_valid_ipv4(ip):
                return ip
        return "127.0.0.1"

    @staticmethod
    def _is_valid_ipv4(ip: str) -> bool:
        try:
            socket.inet_aton(ip)
            return True
        except socket.error:
            return False

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
            boot_time = datetime.datetime.fromtimestamp(psutil.boot_time())
            uptime_timedelta = datetime.datetime.now() - boot_time

            days = uptime_timedelta.days
            hours, remainder = divmod(uptime_timedelta.seconds, 3600)
            minutes, seconds = divmod(remainder, 60)

            uptime_str = f"{days}d {hours}h {minutes}m"
            logger.debug(f"⏳ System Uptime: {uptime_str}")
            return uptime_str

        except Exception as e:
            logger.exception(f"💥 Failed to get system uptime: {e}")
            return "Error retrieving uptime"