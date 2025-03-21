from typing import Dict, List
from fastapi import WebSocket

class WebSocketManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []
        self.subscriptions: Dict[WebSocket, List[str]] = {}  # Хранит, какие данные нужны клиенту

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        self.subscriptions[websocket] = []

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)
        self.subscriptions.pop(websocket, None)

    async def send_data(self, websocket: WebSocket, data_type: str, data: dict):
        """ Отправляет данные клиенту, если он подписан на этот тип данных """
        if websocket in self.subscriptions and data_type in self.subscriptions[websocket]:
            await websocket.send_json({"type": data_type, "data": data})

    async def broadcast(self, data_type: str, data: dict):
        """ Отправляет данные всем подписанным клиентам """
        for websocket in self.active_connections:
            await self.send_data(websocket, data_type, data)

    async def subscribe(self, websocket: WebSocket, data_types: List[str]):
        """ Клиент подписывается на конкретные типы данных """
        self.subscriptions[websocket] = data_types

ws_manager = WebSocketManager()