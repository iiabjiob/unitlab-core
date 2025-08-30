from typing import Dict, Set, List
from pydantic import BaseModel
from fastapi import WebSocket
from app.core.logger import get_logger

logger = get_logger("ws")

class WebSocketManager:
    _instance = None

    def __init__(self):
        self.active_connections: List[WebSocket] = []
        self.subscriptions: Dict[WebSocket, Set[str]] = {}

    @classmethod
    def get_instance(cls):
        if cls._instance is None:
            cls._instance = cls()
        return cls._instance

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        self.subscriptions[websocket] = set()

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
            self.subscriptions.pop(websocket, None)

    async def send_event(self, websocket: WebSocket, event: BaseModel):
        """Отправка Pydantic-события конкретному клиенту"""
        channel = event.channel
        if channel in self.subscriptions.get(websocket, set()):
            try:
                await websocket.send_json(event.model_dump(mode="json"))
            except RuntimeError as e:
                logger.warning(f"⚠️ Failed to send to WS client: {e}")
                self.disconnect(websocket)

    async def send_to(self, websocket: WebSocket, event: BaseModel):
        await self.send_event(websocket, event)

    async def broadcast(self, event: BaseModel):
        """Рассылка Pydantic-события всем подписчикам"""
        for websocket in list(self.active_connections):
            try:
                await self.send_event(websocket, event)
                logger.debug(f"📡 Broadcast: {event.channel}: {event.model_dump()}")
            except Exception as e:
                logger.warning(f"⚠️ Failed to broadcast to one client: {e}")

    async def subscribe(self, websocket: WebSocket, channels: List[str]):
        self.subscriptions.setdefault(websocket, set()).update(channels)

    async def unsubscribe(self, websocket: WebSocket, channels: List[str]):
        if websocket in self.subscriptions:
            self.subscriptions[websocket].difference_update(channels)
            logger.info(f"✅ Unsubscribed: {channels} → Remaining: {self.subscriptions[websocket]}")

    def has_subscribers(self, channel: str) -> bool:
        return any(channel in subs for subs in self.subscriptions.values())
