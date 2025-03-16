from fastapi import FastAPI
from contextlib import asynccontextmanager
import asyncio

from app.api import wifi, system, time_sync, ntp, health
from app.ws.websocket import router as websocket_router
from app.ws.health import health_status_updater
from app.ws.system_ws import system_info_updater
from app.ws.time_ws import time_sync_updater
from app.core.config import get_settings
from app.core.logger import logger

settings = get_settings()

@asynccontextmanager
async def lifespan(app: FastAPI):
    """ Manages FastAPI application lifecycle """

    logger.info("🚀 Starting FastAPI application...")

    # ✅ Safe startup of the background task
    try:
        asyncio.create_task(health_status_updater())
        asyncio.create_task(system_info_updater())
        asyncio.create_task(time_sync_updater())
    except Exception as e:
        logger.error(f"❌ Failed to start 'system_info_updater': {e}")

    yield

    logger.info("🛑 Shutting down FastAPI application...")

app = FastAPI(
    title=settings.app_name,
    description=settings.description,
    version=settings.app_version,
    debug=settings.debug,
    lifespan=lifespan
)
# Добавляем `app_env` в state, чтобы его можно было использовать внутри приложения
app.state.env = settings.app_env

if app.state.env == "production":
    app.openapi_url = None  # Отключаем Swagger UI

# Logging the router setup
logger.info("🔗 Registering REST API routers...")
app.include_router(wifi.router)
app.include_router(system.router)
app.include_router(time_sync.router)
app.include_router(ntp.router)
app.include_router(health.router)

# Register websockets
logger.info("🔗 Registering WebSocket routers...")
app.include_router(websocket_router)

logger.info(f"✅ FastAPI application is up and running in {app.state.env} mode.")
