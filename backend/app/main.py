import asyncio
from fastapi import FastAPI
from contextlib import asynccontextmanager, suppress

from app.api.api_manager import register_routers
from app.ws.ws_router import router as ws_router

from app.core.startup import check_database_connection

from app.redis.manager import RedisManager
from app.mqtt.manager import MqttManager

from app.background.device_offline import device_offline_checker

from app.core.config import get_settings
from app.core.logger import get_logger

settings = get_settings()
logger = get_logger("core")

@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("🚀 Starting FastAPI application...")

    await check_database_connection()

    await RedisManager.start()
    await MqttManager.start()

    # Logging backgroung tasks
    logger.info("🔗 Registering background tasks...")
    checker_task = asyncio.create_task(device_offline_checker())

    try:
        yield
    finally:
        logger.info("🛑 Shutting down FastAPI application...")
        
        checker_task.cancel()
        with suppress(asyncio.CancelledError):
            await checker_task

        await RedisManager.stop()
        await MqttManager.stop()

app = FastAPI(
    title=settings.app_name,
    description=settings.description,
    version=settings.app_version,
    debug=settings.debug,
    lifespan=lifespan
)

# Logging the router setup
logger.info("🔗 Registering REST API routers...")
register_routers(app)

# Logging the websockets
logger.info("🔗 Registering websockets...")
app.include_router(ws_router)

logger.info(f"✅ FastAPI application is up and running at version {settings.app_version}")

