import asyncio
from fastapi import FastAPI
from contextlib import asynccontextmanager, suppress

from app.api.v1.health.router import router as health_router
from app.api.v1.devices.router import router as devices_router
from app.api.v1.channels.router import router as channels_router
from app.api.v1.switchgears.router import router as switchgears_router
from app.api.v1.sequences.router import router as sequences_router
from app.api.v1.workspaces.router import router as workspaces_router
from app.api.v1.signals.router import router as signals_router
from app.api.v1.signal_sheet.router import router as signal_sheet_router

from app.ws.router import router as ws_router

from app.infrastructure.db.health import wait_for_database as check_database_connection

from app.infrastructure.redis.manager import RedisManager

from app.ws.pubsub_listener import forward_ws_events_from_pubsub
from app.services.sequence_event_forwarder import forward_sequence_events
from app.services.system.worker_health_aggregator import run_worker_health_aggregator

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
    sequence_forwarder_task = asyncio.create_task(forward_sequence_events())
    worker_health_task = asyncio.create_task(run_worker_health_aggregator())
    logger.info("✅ WS forwarder started")
    logger.info("✅ Sequence event forwarder started")
    logger.info("✅ Worker health aggregator started")
    try:
        yield
    finally:
        logger.info("🛑 Shutting down FastAPI application...")

        # Cancel background tasks
        ws_forwarder_task.cancel()
        with suppress(asyncio.CancelledError):
            await ws_forwarder_task
        sequence_forwarder_task.cancel()
        with suppress(asyncio.CancelledError):
            await sequence_forwarder_task
        worker_health_task.cancel()
        with suppress(asyncio.CancelledError):
            await worker_health_task

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
app.include_router(workspaces_router)
app.include_router(switchgears_router)
app.include_router(sequences_router)
app.include_router(signals_router)
app.include_router(signal_sheet_router)

logger.info("✅ REST API routers registered")

# Websockets
logger.info("🔗 Registering websockets...")
app.include_router(ws_router)
logger.info("✅ Websockets registered")

logger.info(f"✅ FastAPI application is up and running at version {settings.app_version}")
