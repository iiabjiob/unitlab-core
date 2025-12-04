from __future__ import annotations

from datetime import datetime
from typing import Optional

from sqlalchemy import select, delete
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.device import Device


class DeviceRepository:
    def __init__(self, db: AsyncSession):
        self.db = db

    # -------------------------------------------
    # Base select helpers
    # -------------------------------------------
    def base(self):
        return select(Device)

    def base_with_channels(self):
        return select(Device).options(selectinload(Device.channels))

    # -------------------------------------------
    # CRUD methods
    # -------------------------------------------
    async def list(self, with_channels: bool = False) -> list[Device]:
        query = self.base_with_channels() if with_channels else self.base()
        result = await self.db.execute(query)
        return result.scalars().all()

    async def get(self, device_id: int, with_channels: bool = False) -> Optional[Device]:
        query = (
            self.base_with_channels() if with_channels else self.base()
        ).where(Device.id == device_id)

        result = await self.db.execute(query)
        return result.scalar_one_or_none()

    async def get_by_unit_id(self, unit_id: str, with_channels: bool = False) -> Optional[Device]:
        query = (
            self.base_with_channels() if with_channels else self.base()
        ).where(Device.unit_id == unit_id)

        result = await self.db.execute(query)
        return result.scalar_one_or_none()

    async def create_if_not_exists(self, data: dict) -> Device:
        # First try to locate existing device
        device = await self.get_by_unit_id(data["unit_id"])
        if device:
            return device

        # Create new
        device = Device(**data)
        self.db.add(device)
        await self.db.commit()
        await self.db.refresh(device)

        return device

    async def update(self, device_id: int, changes: dict) -> Optional[Device]:
        dev = await self.get(device_id, with_channels=True)
        if not dev:
            return None

        for k, v in changes.items():
            setattr(dev, k, v)

        await self.db.commit()
        await self.db.refresh(dev)
        return dev

    async def delete(self, device_id: int) -> bool:
        dev = await self.get(device_id)
        if not dev:
            return False

        await self.db.delete(dev)
        await self.db.commit()
        return True

    # -------------------------------------------
    # Last seen
    # -------------------------------------------
    async def set_last_seen(self, unit_id: str, ts: datetime | None) -> Optional[Device]:
        device = await self.get_by_unit_id(unit_id)
        if not device:
            return None

        device.last_seen_at = ts
        await self.db.commit()
        await self.db.refresh(device)
        return device

    # -------------------------------------------
    # Bulk delete (optimized)
    # -------------------------------------------
    async def delete_many(self, ids: list[int]) -> int:
        if not ids:
            return 0

        stmt = delete(Device).where(Device.id.in_(ids)).returning(Device.id)
        result = await self.db.execute(stmt)
        await self.db.commit()

        deleted_ids = result.scalars().all()
        return len(deleted_ids)
