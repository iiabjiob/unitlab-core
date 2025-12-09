# ws/actions/ao_commands.py
import uuid
from fastapi import WebSocket
from app.services.command_queue_service import enqueue_ao_command
from app.schemas.ws.messages import SetAoCommandMessage

async def handle_set_ao_command(ws: WebSocket, msg: SetAoCommandMessage):
    # Кладём AO-команду в outbound очередь
    await enqueue_ao_command(
        unit_id=msg.unit_id,
        ch=msg.ch,
        value=msg.value,
        correlation_id=str(uuid.uuid4()),  # можно пробросить request_id, если есть
    )

