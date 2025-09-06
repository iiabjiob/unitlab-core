import asyncio
import time
from app.ws.manager import WebSocketManager
from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.events import DeviceHeartbeatEvent
from app.core.utils import to_str
from app.core.logger import get_logger
from app.core.config import get_settings

settings = get_settings()
logger = get_logger("offline_checker")


async def device_offline_checker():
    redis = RedisManager.get_instance()
    ws_manager = WebSocketManager.get_instance()

    while True:
        try:
            all_units = await redis.smembers("devices:all")

            for raw_id in all_units:
                unit_id = to_str(raw_id, "")
                status = await redis.get(f"device:{unit_id}:status")
                last_seen = await redis.get(f"device:{unit_id}:last_seen")

                # --- OFFLINE ---
                if not last_seen and status != "offline":
                    await redis.set(f"device:{unit_id}:status", "offline")

                    type_raw = await redis.get(f"device:{unit_id}:type")
                    type_str = to_str(type_raw, "unknown")

                    event = DeviceHeartbeatEvent(
                        unit_id=unit_id,
                        type=type_str,
                        status="offline",
                        last_seen=int(time.time() * 1000),
                    )
                    await ws_manager.broadcast(event)
                    logger.info(f"Device {unit_id} ({type_str}) went offline")

                # --- ONLINE (оживление) ---
                elif status == "offline" and last_seen:
                    await redis.set(f"device:{unit_id}:status", "online")

                    type_raw = await redis.get(f"device:{unit_id}:type")
                    type_str = to_str(type_raw, "unknown")

                    event = DeviceHeartbeatEvent(
                        unit_id=unit_id,
                        type=type_str,
                        status="online",
                        last_seen=int(time.time() * 1000),
                    )
                    await ws_manager.broadcast(event)
                    logger.info(f"Device {unit_id} ({type_str}) came online")

        except Exception as e:
            logger.error(f"💥 Offline checker error: {e}")

        await asyncio.sleep(settings.check_heartbeat_interval)