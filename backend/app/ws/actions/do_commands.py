import time, uuid
from fastapi import WebSocket
from app.services.device_control_service.do import set_do_command_now
from app.schemas.ws.messages import SetDoCommandMessage
from app.services.event_log_service import EventLogService
from app.infrastructure.db.database import AsyncSessionLocal
from app.schemas.ws.events import EventDirection, EventSource
from app.schemas.ws.messages import WSAction
from app.infrastructure.protocol.modes import Cmd

async def handle_set_do_command(ws: WebSocket, msg: SetDoCommandMessage):
    set_do_command_now(
        unit_id=msg.unit_id,
        mode=msg.mode,
        ch=msg.ch,
        value=msg.value,
        bitmask=msg.bitmask,
        chA=msg.chA,
        chB=msg.chB,
        state2b=msg.state2b,
        pulse_ms=msg.pulse_ms,
    )

    # формируем summary по mode
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

    # логирование
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