import subprocess
import re
from app.core.logger import get_logger

logger = get_logger("TimeSync")

def get_chrony_status() -> tuple[str, str, int | None]:
    """
    Returns (status, source, offset_us).
    status: "synced", "unsynced", or "error".
    source: server address or "local"/error text.
    offset_us: offset in microseconds (int) or None.
    """
    return "synced", "local", 0  # Test stub
    # try:
    #     result = subprocess.run(
    #         ["chronyc", "-n", "sources"],
    #         capture_output=True,
    #         text=True,
    #         check=True,
    #     )

    #     for line in result.stdout.splitlines():
    #         if line.startswith("^*"):  # best selected source
    #             parts = line.split()
    #             address = parts[1]
    #             match = re.search(r"([-+]?\d+)us", line)
    #             offset_us = int(match.group(1)) if match else None
    #             return "synced", address, offset_us

    #     return "unsynced", "local", None
    # except Exception as e:
    #     logger.error(f"💥 get_chrony_status failed: {e}")
    #     return "error", str(e), None
