from fastapi import WebSocket

from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.messages import AcknowledgeCoreDiagnosticsMessage
from app.services.core_diagnostics_incident import acknowledge_incident, record_incident_acknowledgement


async def handle_ack_core_diagnostics(
    ws: WebSocket,
    msg: AcknowledgeCoreDiagnosticsMessage,
) -> None:
    del ws
    hostname = msg.hostname.strip()
    incident_id = msg.incident_id.strip()
    if not hostname or not incident_id:
        return
    async with AsyncSessionLocal() as session:
        await record_incident_acknowledgement(
            session,
            hostname=hostname,
            incident_id=incident_id,
        )
    await acknowledge_incident(
        RedisManager.get_instance(),
        hostname=hostname,
        incident_id=incident_id,
    )
