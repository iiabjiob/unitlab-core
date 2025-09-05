import asyncio
from fastapi import FastAPI
from contextlib import asynccontextmanager, suppress

from app.api.devices   import router as devices_router
from app.api.time      import router as time_router
from app.api.event_log import router as event_log_router
from app.ws.router     import router as ws_router

from app.infrastructure.db.database import engine
from sqlalchemy import text

from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.mqtt.manager import MqttManager

from app.tasks.device_offline_task import device_offline_checker
from app.tasks.tyme_sync_task import time_status_broadcaster

from app.core.message_bus import MessageBus
from app.infrastructure.mqtt.inbound_worker import run_inbound_router_worker
from app.infrastructure.mqtt.outbound_worker import run_outbound_publisher_worker

from app.core.config import get_settings
from app.core.logger import get_logger

# Import all MQTT handlers to ensure they register themselves in the router.
# This line is required for side-effects (do not remove).
from app.infrastructure.mqtt.handlers import bootstrap  # noqa: F401


settings = get_settings()
logger = get_logger("core")


async def check_database_connection():
    """Simple DB health check at startup."""
    try:
        async with engine.begin() as conn:
            await conn.execute(text("SELECT 1"))
            logger.info("✅ Connected to the database!")
    except Exception as e:
        logger.error(f"💥 Database connection failed: {e}")


@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("🚀 Starting FastAPI application...")

    # Healthchecks
    await check_database_connection()

    # Start infrastructure services
    await RedisManager.start()
    await MqttManager.start()

    # Background tasks
    logger.info("🔗 Registering background tasks...")
    checker_task = asyncio.create_task(device_offline_checker())
    time_task = asyncio.create_task(time_status_broadcaster())
    logger.info("✅ Background tasks registered")

    bus = MessageBus.get_instance()
    bus.register_task(asyncio.create_task(run_inbound_router_worker()))
    bus.register_task(asyncio.create_task(run_outbound_publisher_worker()))
    logger.info("✅ Queued workers started")
    try:
        yield
    finally:
        logger.info("🛑 Shutting down FastAPI application...")

        # Cancel background tasks
        checker_task.cancel()
        time_task.cancel()
        with suppress(asyncio.CancelledError):
            await checker_task
            await time_task

        # Stop infrastructure services
        await MessageBus.get_instance().shutdown()
        await RedisManager.stop()
        await MqttManager.stop()


app = FastAPI(
    title=settings.app_name,
    description=settings.description,
    version=settings.app_version,
    debug=settings.debug,
    lifespan=lifespan,
)

# Routers
logger.info("🔗 Registering REST API routers...")
app.include_router(devices_router)
app.include_router(time_router)
app.include_router(event_log_router)
logger.info("✅ REST API routers registered")

# Websockets
logger.info("🔗 Registering websockets...")
app.include_router(ws_router)
logger.info("✅ Websockets registered")

logger.info(f"✅ FastAPI application is up and running at version {settings.app_version}")
