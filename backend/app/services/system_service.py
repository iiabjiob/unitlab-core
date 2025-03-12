from app.services.system_info_service import SystemInfoService
from app.services.resource_usage_service import ResourceUsageService
from app.services.temperature_service import TemperatureService
from app.services.wifi_service import WifiService
from app.core.config import get_settings

class SystemService:
    """ Главный сервис, объединяющий все данные """
    @staticmethod
    def get_system_info():
        return {
            "hostname": SystemInfoService.get_hostname(),
            "ip_address": SystemInfoService.get_ip_address(),
            "os": SystemInfoService.get_os(),
            "uptime": SystemInfoService.get_uptime(),
            "cpu_usage": ResourceUsageService.get_cpu_usage(),
            "ram": ResourceUsageService.get_ram_usage(),
            "disk": ResourceUsageService.get_disk_usage(),
            "temperature": TemperatureService.get_temperature(),
            "wifi": WifiService.get_wifi_info(),
        }

    @staticmethod
    def get_app_version():
        """ Возвращает версию API """
        settings = get_settings()
        return {"app_name": settings.app_name, "version": settings.version}