from fastapi import FastAPI
from contextlib import asynccontextmanager

from app.core.config import get_settings
from app.core.logger import get_logger

logger = get_logger("core")

from app.api.api_manager import register_routers
from app.ws.ws_router import router as ws_router
from app.core.startup import check_database_connection, start_background_tasks, start_mqtt

settings = get_settings()

@asynccontextmanager
async def lifespan(app: FastAPI):
    """ Manages FastAPI application lifecycle """

    logger.info("🚀 Starting FastAPI application...")

    await check_database_connection()
    start_mqtt()
    start_background_tasks()

    yield

    logger.info("🛑 Shutting down FastAPI application...")

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

