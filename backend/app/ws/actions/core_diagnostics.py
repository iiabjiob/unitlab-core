from fastapi import WebSocket

from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.messages import AcknowledgeCoreDiagnosticsMessage
from app.services.core_diagnostics_incident import acknowledge_incident


async def handle_ack_core_diagnostics(
    ws: WebSocket,
    msg: AcknowledgeCoreDiagnosticsMessage,
) -> None:
    del ws
    await acknowledge_incident(
        RedisManager.get_instance(),
        hostname=msg.hostname,
        incident_id=msg.incident_id,
    )
