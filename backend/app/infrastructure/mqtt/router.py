import re
from app.infrastructure.mqtt.handler_registry import registry
from app.core.logger import get_logger

logger = get_logger("mqtt")

class MqttRouter:
    """
    Routes MQTT messages to registered async handlers by topic pattern.
    """
    def __init__(self):
        # compile all topic patterns
        self._routes = []
        for pattern, handler in registry.handlers:
            print(f"[DEBUG] Registered handler: {pattern} -> {handler.__name__}")   
            
            # Convert MQTT wildcards to regex: + => ([^/]+), # => .*
            regex = re.compile("^" + pattern.replace("+", r"([^/]+)").replace("#", r".*") + "$")
            self._routes.append((regex, handler, pattern))

    async def route(self, topic: str, payload: bytes):
        for regex, handler, pattern in self._routes:
            match = regex.match(topic)
            if match:
                logger.info(f"Routing topic '{topic}' to handler '{handler.__name__}' (pattern: '{pattern}')")
                await handler(topic, payload, match)
                return
        logger.warning(f"No handler found for topic: {topic}")
