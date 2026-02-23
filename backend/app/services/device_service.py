from __future__ import annotations

from datetime import datetime, timezone
from typing import Optional

from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.devices.repository import DeviceRepository
from app.models.channel import Channel
from app.models.device import Device
from app.schemas.device_schema import DeviceSchema, DeviceSummary
from app.core.config import get_settings
from app.services.device_presence_service import DevicePresenceService

settings = get_settings()


class DeviceService:
    """Business logic helpers for devices."""

    def __init__(self, db: AsyncSession):
        self.db = db
        self.repo = DeviceRepository(db)

    async def list(self) -> list[DeviceSummary]:
        devices = await self.repo.list(with_channels=False)
        summaries = [self._to_summary(dev) for dev in devices]
        await self._overlay_presence_summaries(summaries)
        return summaries

    async def get(self, device_id: int) -> Optional[DeviceSchema]:
        dev = await self.repo.get(device_id, with_channels=True)
        if not dev:
            return None
        schema = self._to_schema(dev)
        await self._overlay_presence_schema(schema)
        return schema

    async def get_by_unit_id(self, unit_id: str, with_channels: bool = False) -> Optional[DeviceSchema]:
        dev = await self.repo.get_by_unit_id(unit_id, with_channels=with_channels)
        if not dev:
            return None
        schema = self._to_schema(dev)
        await self._overlay_presence_schema(schema)
        return schema

    async def update(self, device_id: int, changes: dict) -> Optional[DeviceSchema]:
        dev = await self.repo.update(device_id, changes)
        if not dev:
            return None
        schema = self._to_schema(dev)
        await self._overlay_presence_schema(schema)
        return schema

    async def delete(self, device_id: int) -> bool:
        return await self.repo.delete(device_id)

    async def delete_many(self, ids: list[int]) -> int:
        unique_ids = list({candidate for candidate in ids if isinstance(candidate, int) and not isinstance(candidate, bool)})
        return await self.repo.delete_many(unique_ids)

    async def register_or_update(
        self,
        unit_id: str,
        num_channels: Optional[int] = None,
        firmware_version: Optional[str] = None,
        device_type: Optional[str] = None,
    ) -> tuple[DeviceSchema, bool]:
        normalized_type = device_type.lower() if device_type else None

        dev = await self.repo.get_by_unit_id(unit_id, with_channels=True)
        created = False
        now = datetime.now(timezone.utc)
        if dev:
            changes = {
                "num_channels": num_channels,
                "firmware_version": firmware_version,
                "device_type": normalized_type,
            }
            if dev.registered_at is None:
                changes["registered_at"] = now
            dev = await self.repo.update(dev.id, changes)
        else:
            payload = {
                "unit_id": unit_id,
                "num_channels": num_channels,
                "firmware_version": firmware_version,
                "device_type": normalized_type,
                "registered_at": now,
            }
            dev = await self.repo.create_if_not_exists(payload)
            created = True

        await self._sync_channels(dev, num_channels, normalized_type)
        await self.db.refresh(dev, ["channels"])
        schema = self._to_schema(dev)
        await self._overlay_presence_schema(schema)
        return schema, created

    async def set_last_seen(self, unit_id: str, ts_ms: Optional[int]) -> Optional[DeviceSchema]:
        timestamp = None
        if ts_ms is not None:
            timestamp = datetime.fromtimestamp(ts_ms / 1000, tz=timezone.utc)
        dev = await self.repo.set_last_seen(unit_id, timestamp)
        if not dev:
            return None
        schema = self._to_schema(dev)
        await self._overlay_presence_schema(schema)
        return schema

    async def _sync_channels(self, dev: Device, num_channels: Optional[int], dev_type: Optional[str]):
        if not hasattr(dev, "channels") or dev.channels is None:
            await self.db.refresh(dev, ["channels"])
        wrote_changes = False

        if dev_type:
            for ch in dev.channels:
                if ch.channel_type != dev_type:
                    ch.channel_type = dev_type
                    wrote_changes = True

        if num_channels is None:
            if wrote_changes:
                await self.db.commit()
            return

        existing = {ch.channel_index for ch in dev.channels}
        for idx in range(num_channels):
            if idx not in existing:
                channel = Channel(
                    device_id=dev.id,
                    channel_index=idx,
                    channel_type=dev_type,
                )
                self.db.add(channel)
                wrote_changes = True

        to_delete = [ch for ch in dev.channels if ch.channel_index >= num_channels]
        for ch in to_delete:
            await self.db.delete(ch)
            wrote_changes = True

        if wrote_changes:
            await self.db.commit()

    def _to_schema(self, dev) -> DeviceSchema:
        schema = DeviceSchema.model_validate(dev)
        schema.last_seen = dev.last_seen
        schema.registered_at = dev.registered_at_ms
        schema.status = self._derive_status(schema.last_seen)
        return schema

    def _to_summary(self, dev) -> DeviceSummary:
        summary = DeviceSummary.model_validate(dev)
        summary.last_seen = dev.last_seen
        summary.registered_at = dev.registered_at_ms
        summary.status = self._derive_status(summary.last_seen)
        return summary

    def _derive_status(self, last_seen: Optional[int]) -> str:
        if last_seen is None:
            return "offline"
        ttl_ms = settings.heartbeat_ttl * 1000
        now_ms = int(datetime.now(timezone.utc).timestamp() * 1000)
        return "online" if now_ms - last_seen <= ttl_ms else "offline"

    async def _overlay_presence_schema(self, schema: DeviceSchema) -> None:
        presence = await DevicePresenceService().get_presence(schema.unit_id)
        schema.status = "online" if presence.online else "offline"
        if presence.last_seen_ms is not None:
            schema.last_seen = presence.last_seen_ms

    async def _overlay_presence_summaries(self, summaries: list[DeviceSummary]) -> None:
        if not summaries:
            return
        presence_map = await DevicePresenceService().get_presence_map(summary.unit_id for summary in summaries)
        for summary in summaries:
            presence = presence_map.get(summary.unit_id)
            if presence is None:
                continue
            summary.status = "online" if presence.online else "offline"
            if presence.last_seen_ms is not None:
                summary.last_seen = presence.last_seen_ms
