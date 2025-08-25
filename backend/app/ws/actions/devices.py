from fastapi import WebSocket
from app.services.device_control_service import scan_devices_now, request_state_now
from app.schemas.ws.messages import ScanDevicesMessage, RequestStateMessage

async def handle_scan_devices(ws: WebSocket, msg: ScanDevicesMessage):
    scan_devices_now()

async def handle_get_states(ws: WebSocket, msg: RequestStateMessage):
    request_state_now(msg.type, msg.unit_id)
