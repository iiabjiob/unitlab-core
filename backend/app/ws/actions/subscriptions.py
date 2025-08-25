from fastapi import WebSocket
from inspect import iscoroutinefunction
from app.ws.manager import WebSocketManager
from app.ws.channel_registry import get_channel
from app.schemas.ws.messages import WsSubscribeMessage, WsUnsubscribeMessage

ws_manager = WebSocketManager.get_instance()

async def handle_subscribe(ws: WebSocket, msg: WsSubscribeMessage):
    await ws_manager.subscribe(ws, msg.channels)

    for channel in msg.channels:
        config = get_channel(channel)
        if not config:
            continue

        if callable(config.get("on_subscribe")):
            handler = config["on_subscribe"]
            await handler(ws) if iscoroutinefunction(handler) else handler(ws)

        if callable(config.get("provider")):
            data = config["provider"]()
            await ws_manager.send_to(ws, channel, data)

async def handle_unsubscribe(ws: WebSocket, msg: WsUnsubscribeMessage):
    await ws_manager.unsubscribe(ws, msg.channels)

    for channel in msg.channels:
        config = get_channel(channel)
        if config and callable(config.get("on_unsubscribe")):
            handler = config["on_unsubscribe"]
            await handler(ws) if iscoroutinefunction(handler) else handler(ws)
