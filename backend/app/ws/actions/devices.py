from fastapi import WebSocket
from app.services.device_control_service.scan import scan_devices_now
from app.schemas.ws.messages import ScanDevicesMessage

async def handle_scan_devices(ws: WebSocket, msg: ScanDevicesMessage):
    scan_devices_now()

