import asyncio
from app.ws.websocket_manager import ws_manager
from app.services.system.time_service import TimeSyncService
from app.core.config import get_settings

settings = get_settings()

CHANNEL_NAME = "time_sync"  # WebSocket channel name
INTERVAL = settings.time_sync_interval  # Update interval in seconds

async def send_time_sync():
    """Send periodic time synchronization data via WebSocket"""

    last_sent = None  # Store last sent value

    while True:

        current_data = TimeSyncService.get_time_sync_info()

        if current_data != last_sent:
            
            await ws_manager.broadcast(CHANNEL_NAME, current_data)
            await asyncio.sleep(INTERVAL)
