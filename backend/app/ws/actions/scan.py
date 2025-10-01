# ws/actions/scan.py
import uuid
from fastapi import WebSocket
from app.services.command_queue_service import enqueue_scan_devices
from app.schemas.ws.messages import ScanDevicesMessage

async def handle_scan_devices(ws: WebSocket, msg: ScanDevicesMessage):
    # Просто кладём SCAN в outbound очередь
    await enqueue_scan_devices(correlation_id=str(uuid.uuid4()))
