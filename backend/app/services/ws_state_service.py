import json
from typing import Protocol, cast

from fastapi import WebSocket

from sqlalchemy import select
from sqlalchemy.orm import selectinload
from app.infrastructure.db.database import AsyncSessionLocal
from app.models.device import Device

from app.infrastructure.redis.manager import RedisManager
from app.ws.manager import WebSocketManager
from app.schemas.device_schema import DeviceSchema
from app.schemas.ws.events import DeviceHeartbeatEvent, DeviceRegisterEvent
from app.services.external_ied_availability import list_external_ied_status_snapshots
from app.services.external_ied_planning import list_external_ied_planning_snapshots
from app.services.device_state_service import DeviceStateService
from app.schemas.channel_schema import ChannelSchema
from app.core.utils import to_str
from app.core.logger import get_logger

logger = get_logger("ws")


class _WsStateRedis(Protocol):
    async def get(self, name: str) -> object: ...


JsonObject = dict[str, object]

class WsStateService:
    @staticmethod
    async def send_cached_state_to_ui(unit_id: str, target: WebSocket | None = None) -> None:
        ws_manager = WebSocketManager.get_instance()

        # Pull all cached state events (bitmask + AO) from Redis
        events = await DeviceStateService.get_snapshot(unit_id)

        for event in events:
            if target:
                await ws_manager.send_event(target, event)
            else:
                await ws_manager.broadcast(event)

    @staticmethod
    async def sync_client(ws: WebSocket) -> None:
        """On client connect: REGISTER + STATUS + STATE + TIME"""
        ws_manager = WebSocketManager.get_instance()
        redis = cast(_WsStateRedis, cast(object, RedisManager.get_instance()))

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
                last_seen = int(to_str(last_seen_raw, "0") or "0") if last_seen_raw else None

                # REGISTER event
                schema = DeviceSchema.model_validate(device)
                schema.status = status if status in ("online", "offline") else "offline"
                schema.last_seen = last_seen if last_seen else device.last_seen
                schema.registered_at = device.registered_at_ms
                channels = cast(list[object], cast(object, device.channels))
                schema.channels = [ChannelSchema.model_validate(ch) for ch in channels]
                reg_event = DeviceRegisterEvent.model_validate(schema)
                await ws_manager.send_event(ws, reg_event)

                heartbeat_fast: JsonObject | None = None
                heartbeat_diag: JsonObject | None = None
                hb_fast_raw = await redis.get(f"device:{device.unit_id}:hb_fast")
                hb_diag_raw = await redis.get(f"device:{device.unit_id}:hb_diag")
                try:
                    if hb_fast_raw:
                        encoded = to_str(hb_fast_raw, "{}")
                        parsed_fast: object = cast(object, json.loads(encoded)) if encoded is not None else {}
                        if isinstance(parsed_fast, dict):
                            heartbeat_fast = cast(JsonObject, parsed_fast)
                except Exception:
                    heartbeat_fast = None
                try:
                    if hb_diag_raw:
                        encoded = to_str(hb_diag_raw, "{}")
                        parsed_diag: object = cast(object, json.loads(encoded)) if encoded is not None else {}
                        if isinstance(parsed_diag, dict):
                            heartbeat_diag = cast(JsonObject, parsed_diag)
                except Exception:
                    heartbeat_diag = None

                status_event = DeviceHeartbeatEvent(
                    unit_id=device.unit_id,
                    status=schema.status,
                    last_seen=schema.last_seen or 0,
                    heartbeat_fast=heartbeat_fast,
                    heartbeat_diag=heartbeat_diag,
                )
                await ws_manager.send_event(ws, status_event)

                # STATE snapshot
                await WsStateService.send_cached_state_to_ui(device.unit_id, target=ws)

            for event in await list_external_ied_status_snapshots():
                await ws_manager.send_event(ws, event)
            for event in await list_external_ied_planning_snapshots():
                await ws_manager.send_event(ws, event)
