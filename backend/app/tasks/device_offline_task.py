import asyncio
import time
from app.ws.manager import WebSocketManager
from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.events import DeviceHeartbeatEvent, WSChannel
from app.core.logger import get_logger

logger = get_logger("offline_checker")

CHECK_INTERVAL = 30  # seconds (половина TTL)

async def device_offline_checker():
    redis_client = RedisManager.get_instance()
    ws_manager = WebSocketManager.get_instance()

    while True:
        try:
            # Все unit_id, которые когда-то отмечались онлайн
            online_units = await redis_client.smembers("devices:online")

            for raw_id in online_units:
                unit_id = raw_id.decode() if isinstance(raw_id, (bytes, bytearray)) else str(raw_id)

                # Проверяем TTL у last_seen
                last_seen = await redis_client.get(f"device:{unit_id}:last_seen")
                if not last_seen:
                    # expired → offline
                    await redis_client.srem("devices:online", unit_id)
                    device_type = await redis_client.get(f"device:{unit_id}:type") or "unknown"

                    event = DeviceHeartbeatEvent(
                        unit_id=unit_id,
                        device_type=device_type,
                        status="offline",
                        last_seen=int(time.time() * 1000),
                    )
                    await ws_manager.broadcast(WSChannel.DEVICE_STATE, event.model_dump())
                    logger.info(f"❌ Device {unit_id} ({device_type}) went offline")

        except Exception as e:
            logger.error(f"💥 Offline checker error: {e}")

        await asyncio.sleep(CHECK_INTERVAL)
