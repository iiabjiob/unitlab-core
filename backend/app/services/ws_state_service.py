from sqlalchemy import select
from sqlalchemy.orm import selectinload
from app.infrastructure.db.database import AsyncSessionLocal
from app.models.device import Device

from app.infrastructure.redis.manager import RedisManager
from app.ws.manager import WebSocketManager
from app.schemas.device_schema import DeviceSchema
from app.schemas.ws.events import DeviceRegisterEvent
from app.services.device_state_service import DeviceStateService
from app.schemas.channel_schema import ChannelSchema
from app.core.utils import to_str
from app.core.logger import get_logger

logger = get_logger("ws")

class WsStateService:
    @staticmethod
    async def send_cached_state_to_ui(unit_id: str, target=None):
        ws_manager = WebSocketManager.get_instance()

        # Pull all cached state events (bitmask + AO) from Redis
        events = await DeviceStateService.get_snapshot(unit_id)

        for event in events:
            if target:
                await ws_manager.send_event(target, event)
            else:
                await ws_manager.broadcast(event)

    @staticmethod
    async def sync_client(ws):
        """On client connect: REGISTER + STATUS + STATE + TIME"""
        ws_manager = WebSocketManager.get_instance()
        redis = RedisManager.get_instance()

        async with AsyncSessionLocal() as db:
            result = await db.execute(
                select(Device).options(selectinload(Device.channels))
            )
            devices = result.scalars().all()
            logger.info(f"🔄 Syncing WS client, devices={len(devices)}")

            for device in devices:
                # Status/last_seen loaded from Redis
                status_raw = await redis.get(f"device:{device.unit_id}:status")
                last_seen_raw = await redis.get(f"device:{device.unit_id}:last_seen")

                status = to_str(status_raw, "offline")
                last_seen = int(to_str(last_seen_raw, "0")) if last_seen_raw else None

                # REGISTER event
                schema = DeviceSchema.model_validate(device)
                schema.status = status if status in ("online", "offline") else "offline"
                schema.last_seen = last_seen if last_seen else device.last_seen
                schema.registered_at = device.registered_at_ms
                schema.channels = [ChannelSchema.model_validate(ch) for ch in device.channels]
                reg_event = DeviceRegisterEvent(**schema.model_dump())
                await ws_manager.send_event(ws, reg_event)

                # STATE snapshot
                await WsStateService.send_cached_state_to_ui(device.unit_id, target=ws)