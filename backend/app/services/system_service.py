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
            "version": SystemInfoService.get_app_version(),
            "hostname": SystemInfoService.get_hostname(),
            "ip_address": SystemInfoService.get_ip_address(),
            "os": SystemInfoService.get_os(),
        }

    @staticmethod
    def get_app_version():
        """ Возвращает версию API """
        settings = get_settings()
        return {"app_name": settings.app_name, "version": settings.version}