import time
from sqlalchemy import select
from app.infrastructure.db.database import AsyncSessionLocal
from app.models.device import Device

from app.infrastructure.redis.manager import RedisManager
from app.ws.manager import WebSocketManager
from app.schemas.ws.events import DeviceRegisterEvent
from app.services.device_state_service import DeviceStateService
from app.schemas.ws.events import TimeStatusEvent
from app.services.time_sync import get_chrony_status
from datetime import datetime, timezone
from app.core.utils import to_str
from app.core.logger import get_logger

logger = get_logger("ws")

class WsStateService:
    @staticmethod
    async def send_cached_state_to_ui(unit_id: str, target=None):
        ws_manager = WebSocketManager.get_instance()

        # Берём все события состояния (bitmask + AO) из Redis
        events = await DeviceStateService.get_snapshot(unit_id)

        for event in events:
            if target:
                await ws_manager.send_event(target, event)
            else:
                await ws_manager.broadcast(event)

    @staticmethod
    async def sync_client(ws):
        """При коннекте клиента: REGISTER + STATUS + STATE + TIME + EVENT_LOG"""
        ws_manager = WebSocketManager.get_instance()
        redis = RedisManager.get_instance()

        async with AsyncSessionLocal() as db:
            result = await db.execute(select(Device))
            devices = result.scalars().all()
            logger.info(f"🔄 Syncing WS client, devices={len(devices)}")

            for device in devices:
                # статус/last_seen из Redis
                status_raw = await redis.get(f"device:{device.unit_id}:status")
                last_seen_raw = await redis.get(f"device:{device.unit_id}:last_seen")

                status = to_str(status_raw, "offline")
                last_seen = int(to_str(last_seen_raw, "0")) if last_seen_raw else None

                # REGISTER
                reg_event = DeviceRegisterEvent(
                    unit_id=device.unit_id,
                    type=device.type,
                    firmware_version=float(device.firmware_version) if device.firmware_version else None,
                    channels=device.channels,
                    is_active=device.is_active,
                    status=status if status in ("online", "offline") else "offline",
                    last_seen=last_seen,
                )
                await ws_manager.send_event(ws, reg_event)

                # STATE
                await WsStateService.send_cached_state_to_ui(device.unit_id, target=ws)

            # TIME_STATUS
            try:
                status, source, offset_us = get_chrony_status()
                event = TimeStatusEvent(
                    timestamp=datetime.now(timezone.utc).isoformat(),
                    status=status,
                    source=source,
                    offset_us=offset_us,
                )
                await ws_manager.send_event(ws, event)
            except Exception as e:
                logger.error(f"💥 Failed to sync time status: {e}")
