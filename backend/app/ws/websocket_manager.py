from typing import Dict, Set, List
from fastapi import WebSocket

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

    async def send_data(self, websocket: WebSocket, data_type: str, data: dict):
        if data_type in self.subscriptions.get(websocket, set()):
            await websocket.send_json({"type": data_type, "data": data})

    async def broadcast(self, data_type: str, data: dict):
        for websocket in self.active_connections:
            await self.send_data(websocket, data_type, data)

    async def subscribe(self, websocket: WebSocket, data_types: List[str]):
        self.subscriptions.setdefault(websocket, set()).update(data_types)
    
    async def unsubscribe(self, websocket: WebSocket, data_types: List[str]):
        """
        Removes specified data types from the client's subscription list.
        """
        if websocket in self.subscriptions:
            self.subscriptions[websocket].difference_update(data_types)
            print(f"🚫 Unsubscribed: {data_types} → Remaining: {self.subscriptions[websocket]}")

ws_manager = WebSocketManager()