# app/infrastructure/mqtt/router.py
import re
from app.infrastructure.mqtt.handler_registry import registry
from app.core.logger import get_logger

logger = get_logger("mqtt")

class MqttRouter:
    """
    Routes MQTT messages to registered async handlers by topic pattern.
    """
    def __init__(self):
        self._routes = []
        for pattern, handler in registry.handlers:
            logger.debug(f"🔗 Registered handler: {pattern} -> {handler.__name__}")
            regex = re.compile("^" + pattern.replace("+", r"([^/]+)").replace("#", r".*") + "$")
            self._routes.append((regex, handler, pattern))

    def find_handler(self, topic: str):
        for regex, handler, pattern in self._routes:
            if regex.match(topic):
                return handler, pattern
        return None, None

    @staticmethod
    def extract_unit_id(topic: str) -> str:
        """Extract unit_id from topic (first segment)."""
        return topic.split("/", 1)[0] if "/" in topic else topic
    
    async def route(self, topic: str, payload: bytes):
        handler, pattern = self.find_handler(topic)
        if not handler:
            logger.warning(f"No handler for {topic}")
            return

        # unit_id is always the first segment of a topic
        unit_id = self.extract_unit_id(topic)

        try:
            await handler(topic, payload, unit_id=unit_id)
        except Exception as e:
            logger.error(f"💥 Error while handling {topic}: {e}")
