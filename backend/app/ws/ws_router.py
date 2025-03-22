from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from app.ws.websocket_manager import ws_manager
from app.ws.channel_registry import CHANNELS
from app.core.logger import logger

router = APIRouter(prefix="/ws", tags=["Websocket"])

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
                channels = message.get("channels", [])
                await ws_manager.subscribe(websocket, channels)

                # Отправка данных сразу после подписки
                for channel in channels:
                    if channel in CHANNELS:
                        data = CHANNELS[channel]["provider"]()
                        await ws_manager.send_to(websocket, channel, data)

            elif action == "unsubscribe":
                channels = message.get("channels", [])
                await ws_manager.unsubscribe(websocket, channels)

    except WebSocketDisconnect:
        logger.debug("❌ Client disconnected")  # Disconnection log
        ws_manager.disconnect(websocket)
