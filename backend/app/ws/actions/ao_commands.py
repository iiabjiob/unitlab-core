# ws/actions/ao_commands.py
import time, uuid
from fastapi import WebSocket
from app.services.command_queue_service import enqueue_ao_command
from app.schemas.ws.messages import SetAoCommandMessage
from app.services.event_service import EventService
from app.infrastructure.db.database import AsyncSessionLocal
from app.schemas.ws.events import EventDirection, EventSource
from app.schemas.ws.messages import WSAction

async def handle_set_ao_command(ws: WebSocket, msg: SetAoCommandMessage):
    # Кладём AO-команду в outbound очередь
    await enqueue_ao_command(
        unit_id=msg.unit_id,
        ch=msg.ch,
        value=msg.value,
        correlation_id=str(uuid.uuid4()),  # можно пробросить request_id, если есть
    )

    # Логирование
    async with AsyncSessionLocal() as session:
        await EventService.log_and_broadcast(
            session,
            {
                "event_type": "cmd",
                "ts": int(time.time() * 1000),
                "direction": EventDirection.OUT,
                "source": EventSource.WS_COMMAND.value,
                "payload": {
                    **msg.model_dump(mode="json"),
                    "action": WSAction.SET_AO_COMMAND,
                },
                "message": f"AO command ch={msg.ch}, val={msg.value}",
            },
        )
