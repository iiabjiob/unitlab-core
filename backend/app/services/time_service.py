import subprocess

class TimeSyncService:
    @staticmethod
    def get_ntp_status() -> bool:
        """ Проверяет, синхронизировано ли время через NTP """
        try:
            output = subprocess.run(["timedatectl", "status"], capture_output=True, text=True)
            return "System clock synchronized: yes" in output.stdout
        except Exception:
            return False

    @staticmethod
    def get_ntp_time() -> str:
        """ Получает текущее системное время """
        try:
            output = subprocess.run(["date", "+%Y-%m-%d %H:%M:%S"], capture_output=True, text=True)
            return output.stdout.strip()
        except Exception:
            return "Ошибка получения времени"

    @staticmethod
    def get_ptp_status() -> str:
        """ Проверяет состояние PTP (если используется) """
        try:
            output = subprocess.run(["phc_ctl", "/dev/ptp0", "get"], capture_output=True, text=True)
            return output.stdout.strip()
        except Exception:
            return "Ошибка получения статуса PTP"

    @staticmethod
    def get_ptp_time() -> str:
        """ Получает точное время из PTP """
        try:
            output = subprocess.run(["pmc", "-u", "-b", "0", "get TIME_STATUS_NP"], capture_output=True, text=True)
            return output.stdout.strip()
        except Exception:
            return "Ошибка получения PTP-времени"
