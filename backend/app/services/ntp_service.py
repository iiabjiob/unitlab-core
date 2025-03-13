import os
import subprocess
from app.core.config import get_settings

settings = get_settings()

class NTPService:
    """Сервис для управления NTP-серверами"""

    CHRONY_SOURCES_DIR = "/etc/chrony/sources.d"
    CHRONY_SOURCE_FILE = os.path.join(CHRONY_SOURCES_DIR, "fat-simulator-ntp.sources")

    @staticmethod
    def apply_ntp_config():
        """Обновляет NTP-серверы в `/etc/chrony/sources.d/` и применяет их"""

        ntp_server_1 = settings.ntp_server_1
        ntp_server_2 = settings.ntp_server_2

        config = f"""# Сгенерировано автоматически
            server {ntp_server_1} iburst
            server {ntp_server_2} iburst
            """

        # Проверяем, существует ли `/etc/chrony/sources.d/`
        if not os.path.exists(NTPService.CHRONY_SOURCES_DIR):
            raise FileNotFoundError(f"Directory {NTPService.CHRONY_SOURCES_DIR} not found. Install chrony.")

        # Записываем NTP-серверы в `/etc/chrony/sources.d/custom-ntp.sources`
        try:
            with open(NTPService.CHRONY_SOURCE_FILE, "w") as f:
                f.write(config)

            # Перезагружаем источники времени (без перезапуска `chronyd`)
            subprocess.run(["sudo", "chronyc", "reload", "sources"], check=True)

        except Exception as e:
            raise RuntimeError(f"Ошибка при обновлении NTP-конфигурации: {e}")

    @staticmethod
    def get_ntp_servers():
        """Возвращает текущие NTP-серверы из .env"""
        return {
            "ntp_server_1": settings.ntp_server_1,
            "ntp_server_2": settings.ntp_server_2
        }
