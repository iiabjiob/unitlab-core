from app.services.system_info_service import SystemInfoService

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