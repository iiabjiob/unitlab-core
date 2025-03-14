from fastapi import APIRouter
from app.services.ntp_service import NTPService

router = APIRouter(prefix="/api/ntp", tags=["NTP Settings"])

@router.get("/servers")
def get_ntp_servers():
    """ Возвращает текущие NTP-серверы """
    return NTPService.get_ntp_servers()

@router.post("/apply")
def update_ntp_config():
    """ Применяет новые NTP-серверы """
    NTPService.apply_ntp_config()
    return {"message": "NTP settings updated successfully"}
