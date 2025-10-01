from typing import List
from pydantic import BaseModel
from fastapi import WebSocket
from app.core.logger import get_logger

logger = get_logger("ws")

class WebSocketManager:
    _instance = None

    def __init__(self):
        self.active_connections: List[WebSocket] = []

    @classmethod
    def get_instance(cls):
        if cls._instance is None:
            cls._instance = cls()
        return cls._instance

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

        # Отправляем стейты при подключении
        from app.services.ws_state_service import WsStateService
        await WsStateService.sync_client(websocket)

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def send_event(self, websocket: WebSocket, event: BaseModel):
        """Отправка Pydantic-события конкретному клиенту"""
        try:
            payload = event.model_dump(mode="json")
            logger.debug(f"➡️ Sending WS event: {payload}")
            await websocket.send_json(event.model_dump(mode="json"))
        except RuntimeError as e:
            logger.warning(f"⚠️ Failed to send to WS client: {e}")
            self.disconnect(websocket)

    async def broadcast(self, event: BaseModel):
        """Рассылка Pydantic-события всем подписчикам"""
        for websocket in list(self.active_connections):
            try:
                await self.send_event(websocket, event)
                logger.debug(f"📡 Broadcast: {getattr(event, 'channel', 'n/a')}: {event.model_dump()}")
            except Exception as e:
                logger.warning(f"⚠️ Failed to broadcast to one client: {e}")
