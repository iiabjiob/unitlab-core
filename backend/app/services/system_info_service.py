import platform
import socket
import psutil
import datetime

class SystemInfoService:
    """ Получает информацию о системе """

    @staticmethod
    def get_hostname():
        return socket.gethostname()

    @staticmethod
    def get_ip_address():
        return socket.gethostbyname(socket.gethostname())

    @staticmethod
    def get_os():
        return f"{platform.system()} {platform.version()}"

    @staticmethod
    def get_uptime():
        """ Возвращает аптайм в формате DD:HH:MM:SS """
        uptime_seconds = int(psutil.boot_time())
        uptime_timedelta = datetime.datetime.now() - datetime.datetime.fromtimestamp(uptime_seconds)

        days = uptime_timedelta.days
        hours, remainder = divmod(uptime_timedelta.seconds, 3600)
        minutes, seconds = divmod(remainder, 60)

        return f"{days}d {hours}h {minutes}m {seconds}s"