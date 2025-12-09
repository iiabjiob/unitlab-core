# ws/actions/do_commands.py
import uuid
from fastapi import WebSocket
from app.services.command_queue_service import enqueue_do_command
from app.schemas.ws.messages import SetDoCommandMessage

async def handle_set_do_command(ws: WebSocket, msg: SetDoCommandMessage):
    # Кладём DO-команду в outbound очередь
    await enqueue_do_command(
        unit_id=msg.unit_id,
        mode=msg.mode,
        ch=msg.ch,
        value=msg.value,
        bitmask=msg.bitmask,
        chA=msg.chA,
        chB=msg.chB,
        state2b=msg.state2b,
        pulse_ms=msg.pulse_ms,
        correlation_id=str(uuid.uuid4()),
    )

