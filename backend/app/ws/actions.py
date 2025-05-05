from fastapi import WebSocket
from app.ws.websocket_manager import ws_manager
from app.ws.channel_registry import get_channel
from app.services.mqtt.device_control_service import scan_devices_now, request_states_now, set_pin_now
from inspect import iscoroutinefunction

from app.models.ws_message import WsSubscribeMessage, WsUnsubscribeMessage, ScanDevicesMessage, RequestStatesMessage, SetPinMessage

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

async def handle_scan_devices(ws: WebSocket, msg: ScanDevicesMessage):
    scan_devices_now()

async def handle_request_states(ws: WebSocket, msg: RequestStatesMessage):
    request_states_now(msg.unitId)

async def handle_set_pin(ws: WebSocket, msg: SetPinMessage):
    set_pin_now(msg.unitId, msg.index, msg.value)

# Регистрация хендлеров
ACTION_HANDLERS = {
    "subscribe": handle_subscribe,
    "unsubscribe": handle_unsubscribe,
    "scan_devices": handle_scan_devices,
    "request_states": handle_request_states,
    "set_pin": handle_set_pin,
    # сюда можно добавить другие: "set_pin": handle_set_pin, ...
}
