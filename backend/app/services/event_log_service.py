from app.schemas.ws.events import EventLogEvent
from app.repositories.event_log_repository import EventLogRepository
from app.ws.manager import WebSocketManager

class EventLogService:
    @staticmethod
    async def log_and_broadcast(session, event: dict):
        # 1. Save in DB
        db_event = await EventLogRepository.create(session, event)

        # 2. Broadcast via WS
        ws_event = EventLogEvent(
            id=db_event.id,
            ts=db_event.ts,
            dir=db_event.dir,
            source=db_event.source,
            channel_or_action=db_event.channel_or_action,
            unit_id=db_event.unit_id,
            type=db_event.type,
            summary=db_event.summary,
            payload=db_event.payload,
        )
        await WebSocketManager.get_instance().broadcast(ws_event)

        return db_event