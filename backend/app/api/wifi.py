from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from app.services.wifi_service import WifiService

router = APIRouter(prefix="/wifi", tags=["Wi-Fi"])

class WifiConnectRequest(BaseModel):
    ssid: str
    password: str

@router.get("/scan")
def get_wifi_networks():
    return WifiService.scan_wifi_linux()

@router.post("/connect")
def connect_wifi(request: WifiConnectRequest):
    response = WifiService.connect_to_wifi(request.ssid, request.password)
    if "error" in response:
        raise HTTPException(status_code=400, detail=response["error"])
    return response