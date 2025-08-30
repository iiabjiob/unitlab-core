from fastapi import WebSocket
from app.services.device_control_service.state import request_state_now
from app.schemas.ws.messages import RequestStateMessage

async def handle_get_states(ws: WebSocket, msg: RequestStateMessage):
    request_state_now(msg.type, msg.unit_id, msg.mode, msg.ch)
