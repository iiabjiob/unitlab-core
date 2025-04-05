from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from app.ws.websocket_manager import ws_manager
from app.core.logger import logger
from app.ws.channel_registry import get_channel

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

                # TODO: Подумать как лучше вынести
                # Безопасность: только разрешённые топики
                if not topic.startswith(("do-board-", "di-board-")):
                    logger.warning(f"🚫 Blocked publish to unsafe topic: {topic}")
                    return

                import json
                from app.mqtt.client import client

                payload_str = json.dumps(payload)
                client.publish(topic, payload_str)

                logger.info(f"📡 MQTT publish from WS: {topic} → {payload_str}")

            elif action == "unsubscribe":
                channels = message.get("channels", [])
                await ws_manager.unsubscribe(websocket, channels)

    except WebSocketDisconnect:
        logger.debug("❌ Client disconnected")  # Disconnection log
        ws_manager.disconnect(websocket)
