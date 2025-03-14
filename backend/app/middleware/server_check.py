import time
import httpx
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import JSONResponse
from app.core.logger import logger

class ServerCheckerMiddleware(BaseHTTPMiddleware):
    """ Middleware to check if the server is healthy before processing requests """

    last_check = 0  # Последняя проверка сервера (время в секундах)
    is_server_available = True  # Кэш статуса сервера
    CHECK_INTERVAL = 5  # Проверять сервер раз в 30 секунд

    async def dispatch(self, request: Request, call_next):
        # Проверяем только API-запросы (исключаем WebSocket, статические файлы и health check)
        if request.url.path.startswith("/api/") and request.url.path != "/api/health":
            current_time = time.time()

            # Проверяем, прошло ли достаточно времени с последней проверки
            if current_time - self.last_check > self.CHECK_INTERVAL:
                try:
                    async with httpx.AsyncClient(timeout=1.5) as client:
                        base_url = str(request.base_url).rstrip("/")
                        health_url = f"{base_url}/api/health"

                        response = await client.get(health_url)
                        self.is_server_available = response.status_code == 200
                except httpx.RequestError:
                    self.is_server_available = False
                    logger.error("❌ Could not connect to health check API.")

                # Обновляем время последней проверки
                self.last_check = current_time

            # Если сервер недоступен, сразу возвращаем ошибку
            if not self.is_server_available:
                return JSONResponse(
                    content={"error": "Server is unavailable"},
                    status_code=502
                )

        return await call_next(request)
