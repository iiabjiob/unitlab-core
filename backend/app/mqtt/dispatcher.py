import asyncio
from typing import Set
from app.mqtt.subscription_manager import subscription_manager
from app.core.logger import get_logger

logger = get_logger("mqtt")

async def dispatch(topic: str, payload: str):
    """
    Рассылает MQTT-сообщение всем WS-подписчикам и чистит мёртвые сокеты.
    """
    subscribers = subscription_manager.get_ws_subscribers(topic)

    if not subscribers:
        logger.debug(f"🕸️ No WS subscribers for topic: {topic}")
        return

    logger.debug(f"📤 Dispatching to {len(subscribers)} WS clients: {topic} = {payload}")

    dead_clients: Set = set()

    for subscriber in subscribers:
        try:
            if hasattr(subscriber, "send_json"):
                # Это WebSocket
                await asyncio.wait_for(
                    subscriber.send_json({
                        "type": "mqtt",
                        "channel": topic,
                        "payload": payload,
                    }),
                    timeout=1.5
                )
            elif callable(subscriber):
                # Это async функция
                await asyncio.wait_for(
                    subscriber(topic, payload),
                    timeout=1.5
                )
        except Exception as e:
            logger.warning(f"❌ Failed to dispatch message: {e}")
            if hasattr(subscriber, "send_json"):
                dead_clients.add(subscriber)

    if dead_clients:
        _remove_dead_clients(dead_clients)


def _remove_dead_clients(dead_clients: Set):
    """
    Удаляет умершие WebSocket'ы из всех подписок.
    """
    for topic_key, subscribers in subscription_manager.topic_subscribers.items():
        before = len(subscribers)
        subscribers.difference_update(dead_clients)
        after = len(subscribers)
        if before != after:
            logger.debug(f"🧹 Removed {before - after} dead WS from topic='{topic_key}'")

    # Удаляем пустые ключи
    empty_topics = [key for key, subs in subscription_manager.topic_subscribers.items() if not subs]
    for key in empty_topics:
        del subscription_manager.topic_subscribers[key]
