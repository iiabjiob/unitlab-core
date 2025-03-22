import asyncio
from app.ws.channel_registry import CHANNELS
from app.ws.send_channel_task import send_channel_task 
from app.core.config import get_settings
from app.core.logger import logger

settings = get_settings()


class TaskManager:
    """ Background task manager for handling running processes """
    
    def __init__(self):
        self.tasks = []

    def start_tasks(self):
        """Starts background tasks for all enabled channels."""
        logger.info("🚀 Starting all background tasks...")

        for channel, config in CHANNELS.items():
            if config.get("enabled", False):
                interval = config.get("interval", 10)
                logger.info(f"⏱️  Starting task for channel '{channel}' (every {interval}s)")
                task = asyncio.create_task(send_channel_task(channel, interval))
                self.tasks.append(task)

    def stop_tasks(self):
        """ Stops all background tasks """
        logger.info("🛑 Stopping all background tasks...")
        for task in self.tasks:
            task.cancel()
        self.tasks.clear()

# Create a global instance of TaskManager
task_manager = TaskManager()
