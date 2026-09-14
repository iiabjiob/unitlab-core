# ws/actions/ao_commands.py
from fastapi import WebSocket
from app.schemas.ws.messages import SetAoCommandMessage
from .manual_command_admission import handle_manual_ao

async def handle_set_ao_command(ws: WebSocket, msg: SetAoCommandMessage):
    await handle_manual_ao(ws, msg)
