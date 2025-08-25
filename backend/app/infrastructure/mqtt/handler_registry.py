from typing import Callable, List, Tuple

class HandlerRegistry:
    """
    Registry for MQTT message handlers with topic patterns.
    """
    def __init__(self):
        self._handlers: List[Tuple[str, Callable]] = []

    def mqtt_handler(self, topic_pattern: str):
        """
        Decorator for registering async handler functions for a topic pattern.
        """
        def decorator(func: Callable):
            self._handlers.append((topic_pattern, func))
            return func
        return decorator

    @property
    def handlers(self):
        return list(self._handlers)

# Global singleton for the whole app
registry = HandlerRegistry()
