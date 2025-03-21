import asyncio
from app.ws.tasks.send_time_sync import send_time_sync
from app.core.logger import logger

class TaskManager:
    """ Background task manager for handling running processes """
    
    def __init__(self):
        self.tasks = []

    def start_tasks(self):
        """ Starts all background tasks """
        logger.info("🚀 Starting all background tasks...")
        self.tasks.append(asyncio.create_task(send_time_sync()))

    def stop_tasks(self):
        """ Stops all background tasks """
        logger.info("🛑 Stopping all background tasks...")
        for task in self.tasks:
            task.cancel()
        self.tasks.clear()

# Create a global instance of TaskManager
task_manager = TaskManager()
