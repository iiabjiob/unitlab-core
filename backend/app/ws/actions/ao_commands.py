from fastapi import WebSocket
from app.services.device_control_service.ao import set_ao_command_now
from app.schemas.ws.messages import SetAoCommandMessage

async def handle_set_ao_command(ws: WebSocket, msg: SetAoCommandMessage):
    set_ao_command_now(
        unit_id=msg.unit_id,
        ch=msg.ch,
        value=msg.value,
    )
