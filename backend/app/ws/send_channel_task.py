import asyncio
from app.ws.websocket_manager import ws_manager
from app.ws.channel_registry import CHANNELS

SUBSCRIBE_INTERVAL=1

async def send_channel_task(channel: str, interval: float):
    last_sent = None

    while True:
        if ws_manager.has_subscribers(channel):
            provider = CHANNELS[channel]["provider"]
            data = provider()

            if data != last_sent:
                await ws_manager.broadcast(channel, data)
                last_sent = data

            await asyncio.sleep(interval)
        else:
            await asyncio.sleep(SUBSCRIBE_INTERVAL)
