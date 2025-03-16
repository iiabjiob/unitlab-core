import asyncio
from app.services.websocket_manager import ws_manager
from app.core.logger import logger
from app.core.config import get_settings
settings = get_settings()

CHECK_INTERVAL = int(settings.server_check_interval)  # Интервал проверки

async def health_status_updater():
    """Фоновая задача для мониторинга наличия WebSocket-соединений с клиентами."""
    
    logger.info(f"✅ Background task 'health_status_updater' started with interval {CHECK_INTERVAL} sec.")

    while True:
        connected = ws_manager.has_active_connections("health")

        if connected:
            # ✅ Клиенты подключены, значит, фронт знает о соединении
            logger.debug("📡 WebSocket clients are connected to 'health' channel.")
        else:
            # ❌ Нет подключенных клиентов
            logger.debug("⏳ No WebSocket clients connected to 'health'.")

        await asyncio.sleep(CHECK_INTERVAL)
