from fastapi import APIRouter
from app.services.system_service import SystemService

router = APIRouter(prefix="/system", tags=["System Info"])

class System:

    @router.get("/info")
    def system_info():
        return SystemService.get_system_info()
