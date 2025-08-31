import time, uuid
from fastapi import WebSocket
from app.services.device_control_service.ao import set_ao_command_now
from app.schemas.ws.messages import SetAoCommandMessage
from app.services.event_log_service import EventLogService
from app.infrastructure.db.database import AsyncSessionLocal
from app.schemas.ws.events import EventDirection, EventSource
from app.schemas.ws.messages import WSAction

async def handle_set_ao_command(ws: WebSocket, msg: SetAoCommandMessage):
    set_ao_command_now(
        unit_id=msg.unit_id,
        ch=msg.ch,
        value=msg.value,
    )

    # Логирование через helper
    async with AsyncSessionLocal() as session:
        await EventLogService.log_and_broadcast(session, {
            "id": str(uuid.uuid4()),
            "ts": int(time.time() * 1000),
            "dir": EventDirection.OUT,
            "source": EventSource.WS_COMMAND,
            "channel_or_action": WSAction.SET_AO_COMMAND,
            "unit_id": msg.unit_id,
            "type": "ao",
            "summary": f"AO command ch={msg.ch}, val={msg.value}",
            "payload": msg.model_dump(),
        })
