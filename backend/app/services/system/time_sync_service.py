import subprocess
from datetime import datetime, timezone

class TimeSyncService:

    @staticmethod
    def get_current_time_utc() -> str:
        """Returns the current UTC time in ISO 8601 format."""
        return datetime.now(timezone.utc).isoformat()

    @staticmethod
    def get_sync_source() -> str:
        """Detects the current sync source: PTP, NTP, or LOCAL."""
        if TimeSyncService._is_ptp_synced():
            return "PTP"
        elif TimeSyncService._is_ntp_synced():
            return "NTP"
        else:
            return "LOCAL"

    @staticmethod
    def get_sync_status() -> str:
        """Returns 'synced' if time source is active, otherwise 'unsynced'."""
        source = TimeSyncService.get_sync_source()
        return "synced" if source in ["PTP", "NTP"] else "unsynced"

    @staticmethod
    def get_time_offset_us() -> int:
        """Returns the offset in microseconds from master clock, if available."""
        source = TimeSyncService.get_sync_source()

        if source == "PTP":
            return TimeSyncService._get_ptp_offset_us()
        elif source == "NTP":
            return TimeSyncService._get_ntp_offset_us()
        else:
            return 0

    # Internal helpers

    @staticmethod
    def _is_ptp_synced() -> bool:
        try:
            output = subprocess.check_output(["pgrep", "ptp4l"]).decode().strip()
            return bool(output)
        except Exception:
            return False

    @staticmethod
    def _get_ptp_offset_us() -> int:
        try:
            output = subprocess.check_output(
                ["pmc", "-u", "-b", "0", "GET TIME_STATUS_NP"],
                timeout=1
            ).decode()

            for line in output.splitlines():
                if "offsetFromMaster" in line:
                    # Пример строки: offsetFromMaster  -23
                    return int(line.strip().split()[-1])
        except Exception:
            pass
        return 0

    @staticmethod
    def _is_ntp_synced() -> bool:
        try:
            output = subprocess.check_output(["timedatectl"]).decode().lower()
            return "system clock synchronized: yes" in output
        except Exception:
            return False

    @staticmethod
    def _get_ntp_offset_us() -> int:
        try:
            output = subprocess.check_output(["chronyc", "tracking"]).decode()
            for line in output.splitlines():
                if "Root dispersion" in line:
                    value = float(line.split(":")[1].strip().split()[0])
                    return int(value * 1_000_000)  # seconds -> microseconds
        except Exception:
            pass
        return 0
