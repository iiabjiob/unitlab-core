import asyncio
from app.mqtt.subscription_manager import subscription_manager
from app.core.logger import logger

async def dispatch(topic: str, payload: str):
    """
    Отправляет MQTT-сообщение по WebSocket всем подписанным клиентам.
    Удаляет неактивные WebSocket из подписчиков.
    """
    subscribers = subscription_manager.get_ws_subscribers(topic)

    if not subscribers:
        logger.debug(f"🕸️ No WS subscribers for topic: {topic}")
        return

    logger.debug(f"📤 Dispatching MQTT to {len(subscribers)} WS clients: {topic} = {payload}")

    dead_clients = set()

    for ws in subscribers:
        try:
            await ws.send_json({
                "channel": topic,
                "payload": payload
            })
        except Exception as e:
            logger.warning(f"❌ Failed to send WS message: {e}")
            dead_clients.add(ws)

    # Удаляем умершие сокеты
    if dead_clients:
        for topic_key, subs in subscription_manager.topic_subscribers.items():
            before = len(subs)
            subs.difference_update(dead_clients)
            after = len(subs)
            if before != after:
                logger.debug(f"🧹 Removed {before - after} dead WS from topic='{topic_key}'")

        # Также удалить пустые ключи
        empty_keys = [k for k, v in subscription_manager.topic_subscribers.items() if not v]
        for k in empty_keys:
            del subscription_manager.topic_subscribers[k]
