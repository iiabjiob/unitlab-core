from collections.abc import Awaitable, Callable
class HandlerRegistry:
    """
    Registry for MQTT message handlers with topic patterns.
    """
    def __init__(self):
        self._handlers: list[tuple[str, Callable[..., Awaitable[object]]]] = []

    def mqtt_handler(self, topic_pattern: str):
        """
        Decorator for registering async handler functions for a topic pattern.
        """
        def decorator(func: Callable[..., Awaitable[object]]) -> Callable[..., Awaitable[object]]:
            self._handlers.append((topic_pattern, func))
            return func
        return decorator

    @property
    def handlers(self) -> list[tuple[str, Callable[..., Awaitable[object]]]]:
        return list(self._handlers)

# Global singleton for the whole app
registry = HandlerRegistry()
