from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from pathlib import Path
import subprocess
import re

router = APIRouter(prefix="/api/settings", tags=["Settings"])

CHRONY_CONF = Path("/app/config/chrony.conf")

class TimeSyncSettings(BaseModel):
    ntp1: str | None = None
    ntp2: str | None = None

def write_chrony_conf(servers: list[str]):
    """Перезаписываем chrony.conf c нужными серверами."""
    lines = [
        "driftfile /var/lib/chrony/chrony.drift",
        "makestep 0.1 3",
        "allow all",
    ]
    for s in servers:
        if s:
            lines.append(f"server {s} iburst")
    CHRONY_CONF.write_text("\n".join(lines) + "\n")

def read_chrony_conf() -> list[str]:
    """Возвращаем список серверов из chrony.conf."""
    if not CHRONY_CONF.exists():
        return []
    text = CHRONY_CONF.read_text()
    return re.findall(r"^server\s+(\S+)", text, flags=re.MULTILINE)

@router.get("/timesync")
async def get_timesync():
    servers = read_chrony_conf()
    return {
        "ntp1": servers[0] if len(servers) > 0 else None,
        "ntp2": servers[1] if len(servers) > 1 else None,
        "servers": servers,
    }

@router.post("/timesync")
async def update_timesync(settings: TimeSyncSettings):
    try:
        servers = [s for s in [settings.ntp1, settings.ntp2] if s]
        write_chrony_conf(servers)

        # перезапуск контейнера ntp
        subprocess.run(["docker", "restart", "unitlab-ntp"], check=True)

        return {"status": "ok", "servers": servers}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
