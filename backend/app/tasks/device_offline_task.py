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
    redis_client = RedisManager.get_instance()
    ws_manager = WebSocketManager.get_instance()

    while True:
        try:
            online_units = await redis_client.smembers("devices:online")

            for raw_id in online_units:
                unit_id = to_str(raw_id, "")

                # Проверяем TTL
                last_seen = await redis_client.get(f"device:{unit_id}:last_seen")
                if not last_seen:
                    # expired → offline
                    prev_status = await redis_client.get(f"device:{unit_id}:status")

                    if prev_status != b"offline":  # только при изменении
                        await redis_client.srem("devices:online", unit_id.encode())
                        await redis_client.set(f"device:{unit_id}:status", b"offline")

                        type_raw = await redis_client.get(f"device:{unit_id}:type")
                        type_str = to_str(type_raw, "unknown")

                        event = DeviceHeartbeatEvent(
                            unit_id=unit_id,
                            type=type_str,
                            status="offline",
                            last_seen=int(time.time() * 1000),
                        )
                        await ws_manager.broadcast(event)
                        logger.info(f"Device {unit_id} ({type_str}) went offline")

            # теперь обратная логика: кто-то мог появиться снова (по last_seen обновился)
            all_units = await redis_client.smembers("devices:all")  # множество всех известных
            for raw_id in all_units:
                unit_id = to_str(raw_id, "")
                status = await redis_client.get(f"device:{unit_id}:status")
                last_seen = await redis_client.get(f"device:{unit_id}:last_seen")

                if status == b"offline" and last_seen:
                    # значит устройство снова "ожило"
                    await redis_client.sadd("devices:online", unit_id.encode())
                    await redis_client.set(f"device:{unit_id}:status", b"online")

                    type_raw = await redis_client.get(f"device:{unit_id}:type")
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
