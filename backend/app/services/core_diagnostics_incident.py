from __future__ import annotations

from typing import Any

ACK_TTL_SECONDS = 7 * 24 * 60 * 60


def acknowledgement_key(hostname: str, incident_id: str) -> str:
    return f"core:diagnostics:ack:{hostname.strip()}:{incident_id.strip()}"


async def acknowledge_incident(redis: Any, *, hostname: str, incident_id: str) -> bool:
    hostname = hostname.strip()
    incident_id = incident_id.strip()
    if not hostname or not incident_id:
        return False
    await redis.set(acknowledgement_key(hostname, incident_id), "1", ex=ACK_TTL_SECONDS)
    return True


async def is_incident_acknowledged(redis: Any, *, hostname: str, incident_id: str) -> bool:
    if not hostname.strip() or not incident_id.strip():
        return False
    return bool(await redis.get(acknowledgement_key(hostname, incident_id)))
