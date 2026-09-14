from __future__ import annotations

from datetime import UTC, datetime
import json
from typing import Any

ACK_TTL_SECONDS = 7 * 24 * 60 * 60
ACK_EVENT_STREAM = "core:diagnostics:ack-events"


def acknowledgement_key(hostname: str, incident_id: str) -> str:
    return f"core:diagnostics:ack:{hostname.strip()}:{incident_id.strip()}"


async def acknowledge_incident(redis: Any, *, hostname: str, incident_id: str) -> bool:
    hostname = hostname.strip()
    incident_id = incident_id.strip()
    if not hostname or not incident_id:
        return False
    await redis.set(acknowledgement_key(hostname, incident_id), "1", ex=ACK_TTL_SECONDS)
    await redis.xadd(
        ACK_EVENT_STREAM,
        {
            "event": json.dumps(
                {
                    "event": "core_diagnostics_acknowledged",
                    "hostname": hostname,
                    "incident_id": incident_id,
                    "acknowledged_at": datetime.now(UTC).isoformat(),
                    "actor": "websocket-anonymous",
                },
                separators=(",", ":"),
            )
        },
    )
    return True


async def record_incident_acknowledgement(
    db: Any,
    *,
    hostname: str,
    incident_id: str,
    actor: str = "websocket-anonymous",
) -> None:
    from app.models.core_diagnostics import CoreDiagnosticsAcknowledgement

    db.add(
        CoreDiagnosticsAcknowledgement(
            hostname=hostname.strip(),
            incident_id=incident_id.strip(),
            actor=actor.strip() or "websocket-anonymous",
        )
    )
    await db.commit()


async def is_incident_acknowledged(redis: Any, *, hostname: str, incident_id: str) -> bool:
    if not hostname.strip() or not incident_id.strip():
        return False
    return bool(await redis.get(acknowledgement_key(hostname, incident_id)))
