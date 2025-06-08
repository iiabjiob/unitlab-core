from fastapi import WebSocket
from app.ws.websocket_manager import ws_manager
from app.ws.channel_registry import get_channel
from app.services.mqtt.device_control_service import scan_devices_now, request_state_now, set_do_command_now
from inspect import iscoroutinefunction

from app.models.ws_message import (
    WsSubscribeMessage,
    WsUnsubscribeMessage,
    ScanDevicesMessage,
    RequestStateMessage,
    SetDoCommandMessage,
)

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

# Сканирование устройств
async def handle_scan_devices(ws: WebSocket, msg: ScanDevicesMessage):
    scan_devices_now()

# Запрос состояния устройства (универсально для DO/DI/AO)
async def handle_get_states(ws: WebSocket, msg: RequestStateMessage):
    # type может быть None, тогда логика определяет тип по unit_id
    request_state_now(msg.type, msg.unit_id)

# Управление DO (через новый бинарный протокол)
async def handle_set_do_command(ws: WebSocket, msg: SetDoCommandMessage):
    set_do_command_now(
        unit_id=msg.unit_id,
        mode=msg.mode,
        delay_before_ms=msg.delay_before_ms,
        pulse_ms=msg.pulse_ms,
        repeat=msg.repeat,
        bitmask=msg.bitmask
    )


# Регистрация хендлеров
ACTION_HANDLERS = {
    "subscribe": handle_subscribe,
    "unsubscribe": handle_unsubscribe,
    "scan_devices": handle_scan_devices,
    "get_states": handle_get_states,
    "set_do_command": handle_set_do_command,
    # сюда можно добавить другие: "set_pin": handle_set_pin, ...
}
