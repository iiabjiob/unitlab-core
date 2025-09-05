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
        """
        Return (match, handler) for the given topic or (None, None).
        """
        for regex, handler, pattern in self._routes:
            match = regex.match(topic)
            if match:
                return match, handler
        return None, None

    async def route(self, topic: str, payload: bytes):
        match, handler = self.find_handler(topic)
        if handler:
            await handler(topic, payload, match)
        else:
            logger.warning(f"No handler found for topic: {topic}")
