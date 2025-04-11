from fastapi import APIRouter, HTTPException
import subprocess
import platform
from app.core.config import get_settings

settings = get_settings()

router = APIRouter(prefix="/api/wifi", tags=["Wi-Fi"])

@router.get("/discover")
def discover_wifi():
    if platform.system() != "Linux":
        raise HTTPException(status_code=501, detail="Wi-Fi scan is only supported on Linux.")
    
    try:
        result = subprocess.run(
            ["nmcli", "-t", "-f", "SSID", "device", "wifi", "list"],
            capture_output=True,
            text=True,
            check=True
        )
        ssids = list(set(filter(None, result.stdout.splitlines())))
        fat_networks = sorted(set(
            ssid for ssid in ssids if ssid.startswith(settings.wifi_prefix)
        ))
        return {"devices": fat_networks}
    except subprocess.CalledProcessError as e:
        return {"error": f"Wi-Fi scan failed: {e.stderr}"}
