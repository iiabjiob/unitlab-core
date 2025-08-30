import asyncio
from datetime import datetime, timezone
from app.ws.manager import WebSocketManager
from app.schemas.ws.events import TimeStatusEvent
from app.core.logger import get_logger

logger = get_logger("TS")

CHECK_INTERVAL = 30  # сек
DAILY_BROADCAST = 24 * 60 * 60

async def time_status_broadcaster():
    ws_manager = WebSocketManager.get_instance()
    last_event: TimeStatusEvent | None = None
    last_forced_broadcast = 0
    
    while True:
        try:
            now = datetime.now(timezone.utc).isoformat()
            status = "unsynced"   # TODO: брать из агента/Redis
            source = "local"
            offset_us = None

            event = TimeStatusEvent(
                timestamp=now,
                status=status,
                source=source,
                offset_us=offset_us,
            )

            # send only if changed
            if event.status != getattr(last_event, "status", None) \
               or event.source != getattr(last_event, "source", None):
                await ws_manager.broadcast(event)
                last_event = event
                last_forced_broadcast = asyncio.get_event_loop().time()
                logger.info(f"TimeStatus update: {event.status} via {event.source}")

            # force once a day
            elif asyncio.get_event_loop().time() - last_forced_broadcast > DAILY_BROADCAST:
                await ws_manager.broadcast(event)
                last_forced_broadcast = asyncio.get_event_loop().time()
                logger.debug("TimeStatus daily refresh broadcast")

        except Exception as e:
            logger.error(f"💥 TimeStatus broadcaster error: {e}")

        await asyncio.sleep(CHECK_INTERVAL)
