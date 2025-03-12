from fastapi import APIRouter
from app.services import wifi_service

router = APIRouter(prefix="/wifi", tags=["Wi-Fi"])

@router.get("/scan")
def get_wifi_networks():
    return wifi_service.scan_wifi_linux()
