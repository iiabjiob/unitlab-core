# ws/actions/state.py
import time, uuid
from fastapi import WebSocket
from app.services.command_queue_service import enqueue_request_state
from app.schemas.ws.messages import RequestStateMessage
from app.services.event_log_service import EventLogService
from app.infrastructure.db.database import AsyncSessionLocal
from app.schemas.ws.events import EventDirection, EventSource
from app.schemas.ws.messages import WSAction

async def handle_get_states(ws: WebSocket, msg: RequestStateMessage):
    # Кладём запрос состояния в outbound очередь
    await enqueue_request_state(
        type=msg.type,
        unit_id=msg.unit_id,
        mode=msg.mode,
        ch=msg.ch,
        correlation_id=str(uuid.uuid4()),
    )

    # Log 
    if msg.ch is not None:
        summary = f"REQ state type={msg.type}, mode={msg.mode.name}, ch={msg.ch}"
    else:
        summary = f"REQ state type={msg.type}, mode={msg.mode.name}"
    
    async with AsyncSessionLocal() as session:
        await EventLogService.log_and_broadcast(session, {
            "id": str(uuid.uuid4()),
            "ts": int(time.time() * 1000),
            "dir": EventDirection.OUT,
            "source": EventSource.WS_COMMAND,
            "channel_or_action": WSAction.GET_STATES,
            "unit_id": msg.unit_id,
            "type": msg.type,
            "summary": summary,
            "payload": msg.model_dump(),
        })
