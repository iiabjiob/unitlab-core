import asyncio
from fastapi import FastAPI
from contextlib import asynccontextmanager, suppress

from app.api.devices import router as devices_router
from app.api.time    import router as time_router

from app.ws.router import router as ws_router

from app.core.startup import check_database_connection

from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.mqtt.manager import MqttManager

from app.tasks.device_offline_task import device_offline_checker

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
app.include_router(devices_router)
app.include_router(time_router)

# Logging the websockets
logger.info("🔗 Registering websockets...")
app.include_router(ws_router)

logger.info(f"✅ FastAPI application is up and running at version {settings.app_version}")

