import asyncio
from typing import Callable, Dict
from app.core.logger import get_logger
from app.ws.manager import WebSocketManager

logger = get_logger("ws")

class ChannelTaskManager:
    """Manage background tasks for WS channels with interval+provider (lazy mode)."""

    def __init__(self):
        self.tasks: Dict[str, asyncio.Task] = {}

    async def ensure_task(self, channel: str, interval: int, provider: Callable[[], dict]):
        """Запускает таску для канала, если её ещё нет."""
        if channel in self.tasks:
            return  # уже работает

        ws_manager = WebSocketManager.get_instance()

        async def loop():
            logger.info(f"⏳ Started periodic task for channel {channel} ({interval}s)")
            try:
                while True:
                    # стоп, если подписчиков нет
                    if not ws_manager.has_subscribers(channel):
                        logger.info(f"🛑 No subscribers left for {channel}, stopping task")
                        self.tasks.pop(channel, None)
                        break

                    try:
                        data = provider()
                        await ws_manager.broadcast(channel, data)
                    except Exception as e:
                        logger.error(f"⚠️ Error in provider for {channel}: {e}")

                    await asyncio.sleep(interval)
            except asyncio.CancelledError:
                logger.info(f"⚠️ Task for {channel} cancelled")

        self.tasks[channel] = asyncio.create_task(loop())

    def stop_task(self, channel: str):
        """Остановить таску по каналу."""
        task = self.tasks.pop(channel, None)
        if task:
            task.cancel()

    def stop_all(self):
        """Остановить все таски (например при shutdown)."""
        for task in self.tasks.values():
            task.cancel()
        self.tasks.clear()


# глобальный синглтон
channel_task_manager = ChannelTaskManager()
