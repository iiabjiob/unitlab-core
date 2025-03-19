import subprocess
import platform
from app.core.logger import logger

class TimeSyncService:
    @staticmethod
    def get_ntp_status() -> bool:
        """Checks if the system clock is synchronized via NTP."""
        try:
            if platform.system() != "Linux":
                logger.warning("⚠️ NTP status check is only supported on Linux. Skipping...")
                return False

            output = subprocess.run(["timedatectl", "status"], capture_output=True, text=True, check=True)
            is_synchronized = "System clock synchronized: yes" in output.stdout
            logger.info(f"✅ NTP Status: {'Synchronized' if is_synchronized else 'Not synchronized'}")
            return is_synchronized

        except FileNotFoundError:
            logger.error("❌ `timedatectl` command not found. Ensure NTP is installed.")
        except subprocess.CalledProcessError as e:
            logger.error(f"❌ Failed to execute `timedatectl`: {e}")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error in `get_ntp_status`: {e}")

        return False  # Return False if an error occurs

    @staticmethod
    def get_ntp_time() -> str:
        """Retrieves the current system time via NTP."""
        try:
            output = subprocess.run(["date", "+%Y-%m-%d %H:%M:%S"], capture_output=True, text=True, check=True)
            time = output.stdout.strip()
            logger.info(f"🕒 NTP Time: {time}")
            return time
        except FileNotFoundError:
            logger.error("❌ `date` command not found. Ensure system utilities are available.")
        except subprocess.CalledProcessError as e:
            logger.error(f"❌ Failed to execute `date`: {e}")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error in `get_ntp_time`: {e}")

        return "Failed to retrieve NTP time"

    @staticmethod
    def get_ptp_status() -> bool:
        """Checks if PTP is synchronized."""
        try:
            if platform.system() != "Linux":
                logger.warning("⚠️ PTP status check is only supported on Linux. Skipping...")
                return False

            output = subprocess.run(["phc_ctl", "/dev/ptp0", "get"], capture_output=True, text=True, check=True)
            is_synchronized = bool(output.stdout.strip())

            if is_synchronized:
                logger.info("📡 PTP is active and responding.")
            else:
                logger.warning("⚠️ PTP is installed but returned an empty response.")

            return is_synchronized

        except FileNotFoundError:
            logger.warning("⚠️ `phc_ctl` not found. Ensure PTP is installed.")
        except subprocess.CalledProcessError as e:
            logger.error(f"❌ Failed to execute `phc_ctl`: {e}")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error in `get_ptp_status`: {e}")

        return False  # Return False if an error occurs

    @staticmethod
    def get_ptp_time() -> str:
        """Retrieves the precise time from PTP."""
        try:
            output = subprocess.run(["pmc", "-u", "-b", "0", "get TIME_STATUS_NP"], capture_output=True, text=True, check=True)
            ptp_time = output.stdout.strip()

            if ptp_time:
                logger.info(f"⏱️ PTP Time: {ptp_time}")
                return ptp_time
            else:
                logger.warning("⚠️ PTP time retrieval command executed but returned an empty response.")

        except FileNotFoundError:
            logger.warning("⚠️ `pmc` not found. Ensure PTP is installed.")
        except subprocess.CalledProcessError as e:
            logger.error(f"❌ Failed to execute `pmc`: {e}")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error in `get_ptp_time`: {e}")

        return "Failed to retrieve PTP time"
