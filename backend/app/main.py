from fastapi import FastAPI
import asyncio
from contextlib import asynccontextmanager

from app.core.config import get_settings
from app.core.logger import get_logger

logger = get_logger("core")

from app.api.api_manager import register_routers
from app.ws.ws_router import router as ws_router
from app.core.startup import check_database_connection, start_mqtt_client, stop_mqtt_client, start_redis, stop_redis

settings = get_settings()

@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("🚀 Starting FastAPI application...")

    await check_database_connection()
    await start_redis()
    await start_mqtt_client()

    yield

    logger.info("🛑 Shutting down FastAPI application...")
    await stop_redis()
    await stop_mqtt_client()

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

