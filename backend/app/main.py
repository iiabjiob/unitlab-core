from fastapi import FastAPI
from contextlib import asynccontextmanager
import asyncio

from app.api import wifi, system, time_sync, ntp
from app.ws.websocket import router as websocket_router
from app.ws.system_ws import system_info_updater
from app.core.config import get_settings
from app.core.logger import logger  # ✅ Import logger

settings = get_settings()

@asynccontextmanager
async def lifespan(app: FastAPI):
    """ Function to manage FastAPI application lifecycle """

    logger.info("🚀 Starting FastAPI application...")

    # ✅ Safe startup of the background task
    try:
        asyncio.create_task(system_info_updater())
    except Exception as e:
        logger.error(f"❌ Failed to start 'system_info_updater': {e}")

    yield

    logger.info("🛑 Shutting down FastAPI application...")

app = FastAPI(
    title=settings.app_name,
    description=settings.description,
    version=settings.version,
    debug=settings.debug,
    lifespan=lifespan
)

# Logging the router setup
logger.info("🔗 Registering REST API routers...")
app.include_router(wifi.router)
app.include_router(system.router)
app.include_router(time_sync.router)
app.include_router(ntp.router)

logger.info("🔗 Registering WebSocket routers...")
app.include_router(websocket_router)

logger.info("✅ FastAPI application is up and running.")
