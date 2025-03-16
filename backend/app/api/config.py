from fastapi import APIRouter
from app.core.config import get_settings

settings = get_settings()

router = APIRouter(prefix="/api", tags=["Config"])

@router.get("/config")
async def get_config():
    """ Return .env """
    return settings