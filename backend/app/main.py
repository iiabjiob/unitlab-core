import asyncio
from fastapi import FastAPI
from contextlib import asynccontextmanager, suppress

from app.api.v1.health.router import router as health_router
from app.api.v1.devices.router import router as devices_router
from app.api.v1.channels.router import router as channels_router
from app.api.v1.switchgears.router import router as switchgears_router
from app.api.v1.sequences.router import router as sequences_router
from app.api.v1.projects.router import router as projects_router
from app.api.v1.tests.router import router as tests_router

from app.ws.router import router as ws_router

from app.infrastructure.db.health import wait_for_database as check_database_connection

from app.infrastructure.redis.manager import RedisManager

from app.ws.pubsub_listener import forward_ws_events_from_pubsub

from app.core.config import get_settings
from app.core.logger import get_logger

# Import all MQTT handlers to ensure they register themselves in the router.
# This line is required for side-effects (do not remove).
from app.infrastructure.mqtt.handlers import bootstrap  # noqa: F401


settings = get_settings()
logger = get_logger("core")

@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("🚀 Starting FastAPI application...")

    # Healthchecks (skip heavy DB probe in non-production environments)
    if settings.app_env.lower() == "production":
        await check_database_connection()
    else:
        logger.info("⏩ Skipping DB readiness probe in %s mode", settings.app_env)

    # Start infrastructure services
    await RedisManager.start()

    # Background tasks
    logger.info("🔗 Registering background tasks...")
    ws_forwarder_task = asyncio.create_task(forward_ws_events_from_pubsub())
    logger.info("✅ WS forwarder started")
    try:
        yield
    finally:
        logger.info("🛑 Shutting down FastAPI application...")

        # Cancel background tasks
        ws_forwarder_task.cancel()
        with suppress(asyncio.CancelledError):
            await ws_forwarder_task

        # Stop infrastructure services
        await RedisManager.stop()


app = FastAPI(
    title=settings.app_name,
    description=settings.description,
    version=settings.app_version,
    debug=settings.debug,
    lifespan=lifespan,
)

# Routers
logger.info("🔗 Registering REST API routers...")
app.include_router(health_router)
app.include_router(devices_router)
app.include_router(channels_router)
app.include_router(projects_router)
app.include_router(switchgears_router)
app.include_router(sequences_router)
app.include_router(tests_router)

logger.info("✅ REST API routers registered")

# Websockets
logger.info("🔗 Registering websockets...")
app.include_router(ws_router)
logger.info("✅ Websockets registered")

logger.info(f"✅ FastAPI application is up and running at version {settings.app_version}")