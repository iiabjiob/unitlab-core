from typing import Dict, Set, List
from fastapi import WebSocket
from app.core.logger import logger

class WebSocketManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []
        self.subscriptions: Dict[WebSocket, Set[str]] = {}

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        self.subscriptions[websocket] = set()

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)
        self.subscriptions.pop(websocket, None)

    async def send_data(self, websocket: WebSocket, channel: str, payload: dict):
        if channel in self.subscriptions.get(websocket, set()):
            await websocket.send_json({
                "channel": channel,
                "payload": payload
            })
    
    async def send_to(self, websocket: WebSocket, channel: str, payload: dict):
        await self.send_data(websocket, channel, payload)


    async def broadcast(self, channel: str, payload: dict):
        for websocket in self.active_connections:
            await self.send_data(websocket, channel, payload)


    async def subscribe(self, websocket: WebSocket, channels: List[str]):
        self.subscriptions.setdefault(websocket, set()).update(channels)
    
    async def unsubscribe(self, websocket: WebSocket, channels: List[str]):
        """
        Removes specified data types from the client's subscription list.
        """
        if websocket in self.subscriptions:
            self.subscriptions[websocket].difference_update(channels)
            logger.info(f"🚫 Unsubscribed: {channels} → Remaining: {self.subscriptions[websocket]}")
    
    def has_subscribers(self, data_type: str) -> bool:
        """ Checks if at least one client is subscribed to the given data type """
        if not self.subscriptions:  # ✅ Быстрая проверка, есть ли клиенты вообще
            return False
        return any(data_type in subs for subs in self.subscriptions.values())

ws_manager = WebSocketManager()