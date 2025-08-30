from fastapi import WebSocket
from app.services.device_control_service.do import set_do_command_now
from app.schemas.ws.messages import SetDoCommandMessage

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
        delay_before_ms=msg.delay_before_ms,
        pulse_ms=msg.pulse_ms,
        repeat=msg.repeat,
    )