from __future__ import annotations

from datetime import UTC, datetime
import json
from collections.abc import Mapping
from typing import Protocol

from sqlalchemy.ext.asyncio import AsyncSession

ACK_TTL_SECONDS = 7 * 24 * 60 * 60
ACK_EVENT_STREAM = "core:diagnostics:ack-events"


class DiagnosticsRedisClient(Protocol):
    async def set(self, name: str, value: object, **kwargs: object) -> object: ...

    async def xadd(self, name: str, fields: Mapping[str, object], **kwargs: object) -> object: ...

    async def get(self, name: str) -> object: ...


def acknowledgement_key(hostname: str, incident_id: str) -> str:
    return f"core:diagnostics:ack:{hostname.strip()}:{incident_id.strip()}"


async def acknowledge_incident(redis: DiagnosticsRedisClient, *, hostname: str, incident_id: str) -> bool:
    hostname = hostname.strip()
    incident_id = incident_id.strip()
    if not hostname or not incident_id:
        return False
    _ = await redis.set(acknowledgement_key(hostname, incident_id), "1", ex=ACK_TTL_SECONDS)
    _ = await redis.xadd(
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
    db: AsyncSession,
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


async def is_incident_acknowledged(redis: DiagnosticsRedisClient, *, hostname: str, incident_id: str) -> bool:
    if not hostname.strip() or not incident_id.strip():
        return False
    return bool(await redis.get(acknowledgement_key(hostname, incident_id)))
