from fastapi import APIRouter
from app.core.config import get_settings

settings = get_settings()

router = APIRouter(prefix="/api", tags=["Config"])

@router.get("/config")
async def get_config():
    """ Return settings from .env """
    return {
        "app_name": settings.app_name,
        "app_version": settings.app_version,
        "app_env": settings.app_env,
        "description": settings.description,
        "debug": settings.debug,
    }