import os
import subprocess
from app.core.logger import logger

class NTPService:
    """Service for managing NTP servers"""

    CHRONY_SOURCES_DIR = "/etc/chrony/sources.d"
    CHRONY_SOURCE_FILE = os.path.join(CHRONY_SOURCES_DIR, "unitlab-ntp.sources")
    DEFAULT_SERVERS = ("pool.ntp.org", "time.google.com")

    @staticmethod
    def apply_ntp_config(servers: list[str] | tuple[str, ...] | None = None):
        """Updates NTP servers in `/etc/chrony/sources.d/` and applies changes."""

        normalized = [str(item).strip() for item in (servers or NTPService.DEFAULT_SERVERS) if str(item).strip()]
        if not normalized:
            normalized = list(NTPService.DEFAULT_SERVERS)

        logger.info("🔄 Applying NTP configuration with servers: %s", ", ".join(normalized))

        lines = ["# Auto-generated configuration"]
        lines.extend([f"server {server} iburst" for server in normalized])
        config = "\n".join(lines) + "\n"

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
        """Returns current default NTP servers."""
        servers = {
            "servers": list(NTPService.DEFAULT_SERVERS),
        }
        logger.debug(f"📡 Current NTP servers: {servers}")
        return servers
