import asyncio
import time
from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.events import DeviceHeartbeatEvent
from app.core.utils import to_str
from app.core.logger import get_logger
from app.core.config import get_settings
from app.core.events.ws_event_publisher import WsEventPublisher
from app.services.worker_health import is_clock_adjustment_grace_active

settings = get_settings()
logger = get_logger("offline_checker")
OFFLINE_CONFIRMATION_CHECKS = 2


async def device_offline_checker():
    redis = RedisManager.get_instance()
    missing_heartbeat_checks: dict[str, int] = {}

    while True:
        try:
            all_units = await redis.smembers("devices:all")
            clock_adjustment_grace = await is_clock_adjustment_grace_active(redis)

            for raw_id in all_units:
                unit_id = to_str(raw_id, "")
                status_raw = await redis.get(f"device:{unit_id}:status")
                status = to_str(status_raw)
                last_seen = await redis.get(f"device:{unit_id}:last_seen")

                # --- OFFLINE ---
                if not last_seen and status != "offline":
                    if clock_adjustment_grace:
                        missing_heartbeat_checks.pop(unit_id, None)
                        continue
                    missing_heartbeat_checks[unit_id] = missing_heartbeat_checks.get(unit_id, 0) + 1
                    if missing_heartbeat_checks[unit_id] < OFFLINE_CONFIRMATION_CHECKS:
                        continue
                    # Avoid racing a heartbeat that arrived while this unit was being checked.
                    if await redis.get(f"device:{unit_id}:last_seen"):
                        missing_heartbeat_checks.pop(unit_id, None)
                        continue
                    missing_heartbeat_checks.pop(unit_id, None)
                    await redis.set(f"device:{unit_id}:status", "offline")
 
                    event = DeviceHeartbeatEvent(
                        unit_id=unit_id,
                        status="offline",
                        last_seen=int(time.time() * 1000),
                    )
                    await WsEventPublisher.publish(event)
                    logger.info(f"Device {unit_id} went offline")

                # --- ONLINE (revival) ---
                elif status == "offline" and last_seen:
                    missing_heartbeat_checks.pop(unit_id, None)
                    await redis.set(f"device:{unit_id}:status", "online")

                    event = DeviceHeartbeatEvent(
                        unit_id=unit_id,
                        status="online",
                        last_seen=int(time.time() * 1000),
                    )
                    await WsEventPublisher.publish(event)
                    logger.info(f"Device {unit_id} came online")
                else:
                    missing_heartbeat_checks.pop(unit_id, None)

        except Exception as e:
            logger.error(f"💥 Offline checker error: {e}")

        await asyncio.sleep(settings.check_heartbeat_interval)
