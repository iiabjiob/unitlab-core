import asyncio
from app.mqtt.subscription_manager import subscription_manager
from app.core.logger import logger

async def dispatch(topic: str, payload: str):
    """
    Отправляет MQTT-сообщение по WebSocket всем подписанным клиентам.
    """
    subscribers = subscription_manager.get_ws_subscribers(topic)

    if not subscribers:
        logger.debug(f"🕸️ No WS subscribers for topic: {topic}")
        return

    logger.debug(f"📤 Dispatching MQTT to {len(subscribers)} WS clients: {topic} = {payload}")

    for ws in subscribers:
        try:
            await ws.send_json({
                "channel": topic,
                "payload": payload
            })
        except Exception as e:
            logger.warning(f"❌ Failed to send WS message: {e}")
