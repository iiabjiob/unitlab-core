import os
import subprocess
from app.core.config import get_settings

settings = get_settings()

class NTPService:
    """Сервис для управления NTP-серверами"""

    CHRONY_CONFIG_PATH = "/etc/chrony/chrony.conf"

    @staticmethod
    def apply_ntp_config():
        """ Генерирует новый конфиг NTP и применяет его """
        ntp_server_1 = settings.ntp_server_1
        ntp_server_2 = settings.ntp_server_2

        config = f"""# Сгенерировано автоматически
            server {ntp_server_1} iburst
            server {ntp_server_2} iburst
            """
        # Проверяем, существует ли конфигурационный файл
        if not os.path.exists(NTPService.CHRONY_CONFIG_PATH):
            raise FileNotFoundError(f"File {NTPService.CHRONY_CONFIG_PATH} not fount. Install chrony.")

        # Перезаписываем конфигурационный файл
        try:
            with open(NTPService.CHRONY_CONFIG_PATH, "w") as f:
                f.write(config)

            subprocess.run(["sudo", "systemctl", "restart", "chrony"], check=True)
           
        except Exception as e:
            raise RuntimeError(f"Ошибка при обновлении NTP-конфигурации: {e}")

    @staticmethod
    def get_ntp_servers():
        """ Возвращает текущие NTP-серверы из .env """
        return {
            "ntp_server_1": settings.ntp_server_1,
            "ntp_server_2": settings.ntp_server_2
        }
