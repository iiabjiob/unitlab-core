from fastapi import WebSocket
from inspect import iscoroutinefunction
from app.ws.manager import WebSocketManager
from app.ws.channel_registry import get_channel
from app.ws.channel_task_manager import channel_task_manager
from app.schemas.ws.messages import WsSubscribeMessage, WsUnsubscribeMessage

ws_manager = WebSocketManager.get_instance()

async def handle_subscribe(ws: WebSocket, msg: WsSubscribeMessage):
    await ws_manager.subscribe(ws, msg.channels)

    for channel in msg.channels:
        config = get_channel(channel)
        if not config:
            continue

        # вызываем кастомный on_subscribe (если есть)
        if callable(config.get("on_subscribe")):
            handler = config["on_subscribe"]
            await handler(ws) if iscoroutinefunction(handler) else handler(ws)

        # отправляем первое значение от провайдера
        if callable(config.get("provider")):
            data = config["provider"]()
            await ws_manager.send_to(ws, channel, data)
        
        # запускаем фоновую задачу (если есть interval)
        if callable(config.get("provider")) and config.get("interval"):
            await channel_task_manager.ensure_task(
                channel,
                config["interval"],
                config["provider"],
            )

async def handle_unsubscribe(ws: WebSocket, msg: WsUnsubscribeMessage):
    await ws_manager.unsubscribe(ws, msg.channels)

    for channel in msg.channels:
        config = get_channel(channel)
        if config and callable(config.get("on_unsubscribe")):
            handler = config["on_unsubscribe"]
            await handler(ws) if iscoroutinefunction(handler) else handler(ws)
