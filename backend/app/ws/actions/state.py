# ws/actions/state.py
import time, uuid
from fastapi import WebSocket
from app.services.command_queue_service import enqueue_request_state
from app.schemas.ws.messages import RequestStateMessage
from app.services.event_service import EventService
from app.infrastructure.db.database import AsyncSessionLocal
from app.schemas.ws.events import EventDirection, EventSource
from app.schemas.ws.messages import WSAction

async def handle_get_states(ws: WebSocket, msg: RequestStateMessage):
    # Кладём запрос состояния в outbound очередь
    await enqueue_request_state(
        unit_id=msg.unit_id,
        mode=msg.mode,
        ch=msg.ch,
        correlation_id=str(uuid.uuid4()),
    )

    # Log 
    if msg.ch is not None:
        summary = f"REQ state, mode={msg.mode.name}, ch={msg.ch}"
    else:
        summary = f"REQ state, mode={msg.mode.name}"
    
    async with AsyncSessionLocal() as session:
        await EventService.log_and_broadcast(
            session,
            {
                "event_type": "cmd",
                "ts": int(time.time() * 1000),
                "direction": EventDirection.OUT,
                "source": EventSource.WS_COMMAND.value,
                "payload": {
                    **msg.model_dump(mode="json"),
                    "action": WSAction.GET_STATES,
                },
                "message": summary,
            },
        )
