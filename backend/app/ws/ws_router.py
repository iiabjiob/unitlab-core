from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from app.ws.websocket_manager import ws_manager
from app.ws.channel_registry import get_channel
from app.mqtt.topics import is_allowed_publish_topic
from app.core.logger import get_logger

logger = get_logger("ws")

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

                for channel in channels:
                    config = get_channel(channel)
                    if config and callable(config.get("on_subscribe")):
                        config["on_subscribe"](websocket)

                    provider = config.get("provider")
                    if callable(provider):
                        data = provider()
                        await ws_manager.send_to(websocket, channel, data)
                    else:
                        logger.debug(f"⏩ Channel '{channel}' is push-only, skipping initial send.")
            
            elif action == "publish":
                topic = message.get("topic")
                payload = message.get("payload")

                if not topic or payload is None:
                    logger.warning(f"❌ Invalid publish request: {message}")
                    return

                # Безопасность: только разрешённые топики
                if not is_allowed_publish_topic(topic):
                    logger.warning(f"🚫 Blocked publish to unsafe topic: {topic}")
                    return

                from app.mqtt.publisher import publish_json
                publish_json(topic, payload)

                logger.info(f"📡 Publish MQTT from WS: {topic} → {payload}")

            elif action == "unsubscribe":
                channels = message.get("channels", [])
                await ws_manager.unsubscribe(websocket, channels)

    except WebSocketDisconnect:
        logger.debug("❌ Client disconnected")  # Disconnection log
        ws_manager.disconnect(websocket)
