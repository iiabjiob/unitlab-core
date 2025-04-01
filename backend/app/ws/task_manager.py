import asyncio
from app.ws.channel_registry import CHANNELS
from app.ws.send_channel_task import send_channel_task 
from app.core.config import get_settings
from app.core.logger import logger

settings = get_settings()

class TaskManager:
    """Background task manager for periodic WS channels."""

    def __init__(self):
        self.tasks: list[asyncio.Task] = []

    def start_tasks(self):
        """Start periodic tasks only for channels with a provider."""
        logger.info("🚀 Starting all background tasks...")

        for name, config in CHANNELS.items():
            provider = config.get("provider")
            if not config.get("enabled", False):
                continue
            if provider is None:
                logger.debug(f"⏩ Skipping channel '{name}' (no provider, push-only)")
                continue

            interval = config.get("interval", 10)
            logger.info(f"⏱️  Starting task for channel '{name}' (every {interval}s)")

            task = asyncio.create_task(self._wrap_task(name, interval))
            self.tasks.append(task)

    def stop_tasks(self):
        """Cancel all running tasks."""
        logger.info("🛑 Stopping all background tasks...")
        for task in self.tasks:
            task.cancel()
        self.tasks.clear()

    async def _wrap_task(self, channel: str, interval: int):
        """Wraps the task with error handling and graceful cancellation."""
        try:
            await send_channel_task(channel, interval)
        except asyncio.CancelledError:
            logger.debug(f"⛔ Channel task '{channel}' cancelled.")
        except Exception as e:
            logger.exception(f"💥 Unhandled exception in channel '{channel}': {e}")

task_manager = TaskManager()