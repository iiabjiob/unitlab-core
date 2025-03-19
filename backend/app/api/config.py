from fastapi import APIRouter
from app.core.config import get_settings

settings = get_settings()

router = APIRouter(prefix="/api", tags=["Config"])

@router.get("/config")
async def get_config():
    """ Return settings from .env """
    return {
        "host": settings.host,
        "app_name": settings.app_name,
        "app_version": settings.app_version,
        "app_env": settings.app_env,
        "description": settings.description,
        "debug": settings.debug,
        "health_check_interval": settings.health_check_interval,
    }