# ws/actions/do_commands.py
from fastapi import WebSocket
from app.schemas.ws.messages import SetDoCommandMessage
from .manual_command_admission import handle_manual_do

async def handle_set_do_command(ws: WebSocket, msg: SetDoCommandMessage):
    await handle_manual_do(ws, msg)
