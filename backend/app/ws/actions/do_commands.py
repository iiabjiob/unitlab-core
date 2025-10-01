# ws/actions/do_commands.py
import time, uuid
from fastapi import WebSocket
from app.services.command_queue_service import enqueue_do_command
from app.schemas.ws.messages import SetDoCommandMessage
from app.services.event_log_service import EventLogService
from app.infrastructure.db.database import AsyncSessionLocal
from app.schemas.ws.events import EventDirection, EventSource
from app.schemas.ws.messages import WSAction
from app.infrastructure.protocol.modes import Cmd

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

    # Формируем summary
    if msg.mode == Cmd.SET_SINGLE_BIT:
        summary = f"DO SET_SINGLE_BIT ch={msg.ch}, val={msg.value}"
    elif msg.mode == Cmd.SET_ALL_BIT:
        summary = f"DO SET_ALL_BIT bitmask=0x{msg.bitmask:X}"
    elif msg.mode == Cmd.SET_PAIR_BIT:
        summary = f"DO SET_PAIR_BIT chA={msg.chA}, chB={msg.chB}, state2b={msg.state2b}"
    elif msg.mode == Cmd.SET_PULSE_BIT:
        summary = f"DO SET_PULSE_BIT ch={msg.ch}, val={msg.value}, pulse_ms={msg.pulse_ms}"
    else:
        summary = f"DO command mode={msg.mode.name}"

    # Логирование
    async with AsyncSessionLocal() as session:
        await EventLogService.log_and_broadcast(session, {
            "id": str(uuid.uuid4()),
            "ts": int(time.time() * 1000),
            "dir": EventDirection.OUT,
            "source": EventSource.WS_COMMAND,
            "channel_or_action": WSAction.SET_DO_COMMAND,
            "unit_id": msg.unit_id,
            "type": "do",
            "summary": summary,
            "payload": msg.model_dump(),
        })
