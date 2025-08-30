import asyncio
import time
from app.ws.manager import WebSocketManager
from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.events import DeviceHeartbeatEvent
from app.core.logger import get_logger
from app.core.config import get_settings

settings = get_settings()
logger = get_logger("offline_checker")

async def device_offline_checker():
    redis_client = RedisManager.get_instance()
    ws_manager = WebSocketManager.get_instance()

    while True:
        try:
            online_units = await redis_client.smembers("devices:online")

            for raw_id in online_units:
                unit_id = raw_id.decode() if isinstance(raw_id, (bytes, bytearray)) else str(raw_id)

                # Проверяем TTL
                last_seen = await redis_client.get(f"device:{unit_id}:last_seen")
                if not last_seen:
                    # expired → offline
                    prev_status = await redis_client.get(f"device:{unit_id}:status")

                    if prev_status != b"offline":  # только при изменении
                        await redis_client.srem("devices:online", unit_id)
                        await redis_client.set(f"device:{unit_id}:status", "offline")

                        type = await redis_client.get(f"device:{unit_id}:type") or "unknown"

                        event = DeviceHeartbeatEvent(
                            unit_id=unit_id,
                            type=type.decode() if isinstance(type, (bytes, bytearray)) else str(type),
                            status="offline",
                            last_seen=int(time.time() * 1000),
                        )
                        await ws_manager.broadcast(event)
                        logger.info(f"Device {unit_id} ({type}) went offline")

        except Exception as e:
            logger.error(f"💥 Offline checker error: {e}")

        await asyncio.sleep(settings.check_heartbeat_interval)
