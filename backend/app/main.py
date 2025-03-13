from fastapi import FastAPI
from app.api import wifi, system, time_sync
from contextlib import asynccontextmanager
from app.core.config import get_settings

settings = get_settings()

@asynccontextmanager
async def lifespan(app: FastAPI):
    yield

app = FastAPI(
    title=settings.app_name,
    description=settings.description,
    version=settings.version,
    debug=settings.debug,
    lifespan=lifespan
    )

# Подключаем роутеры
app.include_router(wifi.router)
app.include_router(system.router)
app.include_router(time_sync.router)