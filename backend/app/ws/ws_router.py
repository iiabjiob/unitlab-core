from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from app.ws.websocket_manager import ws_manager
from app.core.logger import logger

router = APIRouter()

@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await ws_manager.connect(websocket)
    logger.debug("✅ New WebSocket client connected!")  # Connection log

    try:
        while True:
            message = await websocket.receive_json()
            logger.debug("📩 Received message from client:", message)  # Log received message
            action = message.get("action")
            
            if action == "subscribe":
                data_types = message.get("data_types", [])
                await ws_manager.subscribe(websocket, data_types)

    except WebSocketDisconnect:
        logger.debug("❌ Client disconnected")  # Disconnection log
        ws_manager.disconnect(websocket)
