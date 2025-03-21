from fastapi import FastAPI
from contextlib import asynccontextmanager
from app.api import wifi, time_sync, ntp, health, config
from app.ws.ws_router import router as ws_router
from app.ws.task_manager import task_manager
from app.core.config import get_settings
from app.db.database import engine
from sqlalchemy.sql import text
from app.core.logger import logger

settings = get_settings()

@asynccontextmanager
async def lifespan(app: FastAPI):
    """ Manages FastAPI application lifecycle """

    logger.info("🚀 Starting FastAPI application...")

    # Проверка соединения с БД при запуске
    try:
        async with engine.begin() as conn:
            await conn.execute(text("SELECT 1"))
            logger.info("✅ Connected to the database!")
    except Exception as e:
        logger.error(f"❌ Database connection failed: {e}")

    # Safe startup of the background task
    try:
        task_manager.start_tasks()
    except Exception as e:
        logger.error(f"❌ Failed to start background tasks: {e}")

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
app.include_router(wifi.router)
app.include_router(time_sync.router)
app.include_router(ntp.router)
app.include_router(health.router)
app.include_router(config.router)

# Logging the websockets
logger.info("🔗 Registering websockets...")
app.include_router(ws_router)

logger.info(f"✅ FastAPI application is up.")
