
import fnmatch

class SubscriptionManager:
    def __init__(self):
        self.topic_subscribers: dict[str, set] = {}

    def subscribe(self, topic: str, ws):
        self.topic_subscribers.setdefault(topic, set()).add(ws)

        from app.mqtt.client import client
        client.subscribe(topic)

        from app.core.logger import logger
        logger.info(f"📡 MQTT subscribed: topic='{topic}', total_clients={len(self.topic_subscribers[topic])}")

    def unsubscribe(self, topic: str, ws):
        if topic in self.topic_subscribers:
            self.topic_subscribers[topic].discard(ws)
            if not self.topic_subscribers[topic]:
                from app.mqtt.client import client
                client.unsubscribe(topic)
                del self.topic_subscribers[topic]

    def get_ws_subscribers(self, topic: str) -> set:
        matched = set()
        for pattern, subscribers in self.topic_subscribers.items():
            # Поддержка # как в MQTT
            mqtt_pattern = pattern.replace("#", "*")
            if fnmatch.fnmatch(topic, mqtt_pattern):
                matched.update(subscribers)
        return matched


# Экземпляр, который можно импортировать
subscription_manager = SubscriptionManager()
