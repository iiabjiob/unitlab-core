import os
import subprocess
from app.core.config import get_settings
from app.core.logger import logger

settings = get_settings()

class NTPService:
    """Service for managing NTP servers"""

    CHRONY_SOURCES_DIR = "/etc/chrony/sources.d"
    CHRONY_SOURCE_FILE = os.path.join(CHRONY_SOURCES_DIR, "unitlab-ntp.sources")

    @staticmethod
    def apply_ntp_config():
        """Updates NTP servers in `/etc/chrony/sources.d/` and applies changes"""

        ntp_server_1 = settings.ntp_server_1
        ntp_server_2 = settings.ntp_server_2

        logger.info(f"🔄 Applying NTP configuration with servers: {ntp_server_1}, {ntp_server_2}")

        config = f"""# Auto-generated configuration
            server {ntp_server_1} iburst
            server {ntp_server_2} iburst
            """

        # Ensure `/etc/chrony/sources.d/` exists
        if not os.path.exists(NTPService.CHRONY_SOURCES_DIR):
            logger.error(f"💥 Directory {NTPService.CHRONY_SOURCES_DIR} not found. Ensure Chrony is installed.")
            raise FileNotFoundError(f"Directory {NTPService.CHRONY_SOURCES_DIR} not found. Install Chrony.")

        # Write NTP servers to the configuration file
        try:
            with open(NTPService.CHRONY_SOURCE_FILE, "w") as f:
                f.write(config)
            logger.info(f"✅ NTP configuration written to {NTPService.CHRONY_SOURCE_FILE}")

            # Reload sources without restarting `chronyd`
            subprocess.run(["sudo", "chronyc", "reload", "sources"], check=True)
            logger.info("🔄 NTP sources reloaded successfully.")

        except subprocess.CalledProcessError as e:
            logger.error(f"💥 Failed to reload NTP sources: {e}")
        except Exception as e:
            logger.exception(f"⚠️ Unexpected error while updating NTP configuration: {e}")

    @staticmethod
    def get_ntp_servers():
        """Returns current NTP servers from environment settings"""
        servers = {
            "ntp_server_1": settings.ntp_server_1,
            "ntp_server_2": settings.ntp_server_2
        }
        logger.debug(f"📡 Current NTP servers: {servers}")
        return servers
