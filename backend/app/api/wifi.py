from fastapi import APIRouter, HTTPException
from services import wifi_service

router = APIRouter(prefix="/wifi", tags=["Wi-Fi"])

@router.get("/mode")
def get_wifi_mode():
    """Возвращает текущий режим Wi-Fi (client или AP)."""
    try:
        mode = wifi_service.get_wifi_mode()
        return {"mode": mode}
    except RuntimeError as e:
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/set_client")
def set_wifi_client():
    """Переключает Raspberry Pi в режим Wi-Fi клиента."""
    try:
        status = wifi_service.set_wifi_client()
        return {"status": status}
    except RuntimeError as e:
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/set_ap")
def set_wifi_ap():
    """Переключает Raspberry Pi в режим точки доступа (AP Mode)."""
    try:
        status = wifi_service.set_wifi_ap()
        return {"status": status}
    except RuntimeError as e:
        raise HTTPException(status_code=500, detail=str(e))
