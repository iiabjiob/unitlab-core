
import fnmatch
from app.core.logger import get_logger
from typing import Callable, Union, Set
from fastapi import WebSocket

logger = get_logger("mqtt")

# Подписчик может быть WebSocket или асинхронной функцией
Subscriber = Union[WebSocket, Callable[[str, dict], None]]

class SubscriptionManager:
    def __init__(self):
        self.topic_subscribers: dict[str, Set[Subscriber]] = {}

    def subscribe(self, topic: str, subscriber: Subscriber):
        self.topic_subscribers.setdefault(topic, set()).add(subscriber)

        from app.mqtt.client import client
        client.subscribe(topic)

        logger.info(f"📡 Subscribed: topic='{topic}', total_subscribers={len(self.topic_subscribers[topic])}")

    def unsubscribe(self, topic: str, subscriber: Subscriber):
        if topic in self.topic_subscribers:
            self.topic_subscribers[topic].discard(subscriber)
            if not self.topic_subscribers[topic]:
                from app.mqtt.client import client
                client.unsubscribe(topic)
                del self.topic_subscribers[topic]

    def get_subscribers(self, topic: str) -> Set[Subscriber]:
        matched = set()
        for pattern, subscribers in self.topic_subscribers.items():
            mqtt_pattern = pattern.replace("#", "*")
            if fnmatch.fnmatch(topic, mqtt_pattern):
                matched.update(subscribers)
        return matched

    async def dispatch(self, topic: str, payload: dict):
        """Отправить сообщение всем подписчикам."""
        subscribers = self.get_subscribers(topic)
        for subscriber in subscribers:
            if hasattr(subscriber, "send_json"):  # Это WebSocket
                try:
                    await subscriber.send_json({
                        "topic": topic,
                        "payload": payload,
                    })
                except Exception as e:
                    logger.warning(f"❗ Failed to send to WebSocket subscriber: {e}")
            elif callable(subscriber):  # Это асинхронная функция-хендлер
                try:
                    await subscriber(topic, payload)
                except Exception as e:
                    logger.warning(f"❗ Failed to call subscriber function: {e}")


# Экземпляр, который можно импортировать
subscription_manager = SubscriptionManager()
