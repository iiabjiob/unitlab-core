# ws/actions/state.py
import uuid
from fastapi import WebSocket
from app.services.command_queue_service import enqueue_request_state
from app.schemas.ws.messages import RequestStateMessage

async def handle_get_states(ws: WebSocket, msg: RequestStateMessage):
    # Push state request into outbound queue
    await enqueue_request_state(
        unit_id=msg.unit_id,
        mode=msg.mode,
        ch=msg.ch,
        correlation_id=str(uuid.uuid4()),
    )
