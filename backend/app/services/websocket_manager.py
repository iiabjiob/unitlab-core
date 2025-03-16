from fastapi import WebSocket, WebSocketDisconnect
from typing import Dict, List

class WebSocketManager:
    """ Менеджер WebSocket-соединений с поддержкой нескольких каналов """
    
    def __init__(self):
        self.active_connections: Dict[str, List[WebSocket]] = {}

    async def connect(self, websocket: WebSocket, topic: str):
        """ Подключение клиента к определенной теме """
        if topic not in self.active_connections:
            self.active_connections[topic] = []
        self.active_connections[topic].append(websocket)

    async def disconnect(self, websocket: WebSocket, topic: str):
        """ Отключение клиента из темы """
        if topic in self.active_connections and websocket in self.active_connections[topic]:
            self.active_connections[topic].remove(websocket)

    async def send_data(self, topic: str, data: dict):
        """ Отправка данных всем клиентам, подписанным на определенный канал """
        if topic in self.active_connections:
            for connection in self.active_connections[topic]:
                try:
                    await connection.send_json(data)
                except WebSocketDisconnect:
                    self.active_connections[topic].remove(connection)
    
    def has_active_connections(self, topic: str) -> bool:
        """ Проверяет, есть ли активные WebSocket-клиенты для данной темы """
        return topic in self.active_connections and len(self.active_connections[topic]) > 0


ws_manager = WebSocketManager()
