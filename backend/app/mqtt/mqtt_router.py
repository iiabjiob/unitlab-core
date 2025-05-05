import asyncio, json
import paho.mqtt.client as mqtt
from typing import Callable, Awaitable

from app.core.logger import get_logger

logger = get_logger("mqtt")

RouteHandler = Callable[[str, dict], Awaitable[None]]

class MQTTRouter:
    def __init__(self, client: mqtt.Client, loop: asyncio.AbstractEventLoop):
        self.client = client
        self.loop = loop   # <- сохраняем нужный loop

    def route(self, topic_filter: str) -> Callable[[RouteHandler], RouteHandler]:
        def decorator(func: RouteHandler) -> RouteHandler:
            # подписка
            self.client.subscribe(topic_filter)

            def _wrapper(client, userdata, msg):
                try:
                    payload = json.loads(msg.payload.decode("utf-8"))
                except json.JSONDecodeError:
                    payload = msg.payload.decode("utf-8")

                # создаём задачу именно в нашем loop
                asyncio.run_coroutine_threadsafe(
                    func(msg.topic, payload),
                    self.loop
                )

            # привязываем
            self.client.message_callback_add(topic_filter, _wrapper)
            
            logger.info("Handler registered for: %s.", topic_filter)
            return func

        return decorator
