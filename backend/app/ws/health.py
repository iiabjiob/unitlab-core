import asyncio
import httpx
from app.core.config import get_settings
from app.services.websocket_manager import ws_manager
from app.core.logger import logger

settings = get_settings()

CHECK_INTERVAL = int (settings.server_check_interval)
HOST = str (settings.host)

async def health_status_updater():
    """Фоновая задача для проверки состояния сервера и отправки данных WebSocket-клиентам."""
    
    logger.info(f"✅ Background task 'health_status_updater' started with interval {CHECK_INTERVAL} sec.")
    server_status = None

    while True:
        if ws_manager.has_active_connections("health"):
            try:
                async with httpx.AsyncClient(timeout=2) as client:
                    response = await client.get(f"http://{HOST}/api/health")
                    new_status = response.status_code == 200
            except httpx.RequestError:
                new_status = False  # Сервер недоступен

            if new_status != server_status:
                server_status = new_status
                message = {"status": "ok" if server_status else "down"}
                
                # ✅ Логируем отправку данных
                logger.debug(f"📡 Sending health status to WebSocket clients: {message}")
                
                await ws_manager.send_data("health", message)

        else:
            logger.debug("⏳ No WebSocket clients connected to 'health'. Waiting...")

        await asyncio.sleep(CHECK_INTERVAL)
