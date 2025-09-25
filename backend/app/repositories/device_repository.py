from datetime import datetime, timezone
from typing import Optional

from sqlalchemy import select
from sqlalchemy.orm import selectinload
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.device import Device
from app.repositories.channel_repository import ChannelRepository


class DeviceRepository:
    """Repository for CRUD operations on Device."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def register_if_not_exists(
        self,
        unit_id: str,
        num_channels: int,
        firmware_version: Optional[str] = None,
        type: Optional[str] = None,
        is_active: bool = True,
    ) -> Device:
        try:
            result = await self.db.execute(
                select(Device).where(Device.unit_id == unit_id)
            )
            device = result.scalar_one_or_none()
            if device:
                return device

            new_device = Device(
                unit_id=unit_id,
                type=type,
                num_channels=num_channels,
                firmware_version=firmware_version,
                is_active=is_active,
                created_at=datetime.now(timezone.utc),
            )

            self.db.add(new_device)
            await self.db.commit()
            await self.db.refresh(new_device)
            return new_device

        except SQLAlchemyError as e:
            await self.db.rollback()
            raise RuntimeError(f"Database error during registration: {e}")

    async def register_or_update(
        self,
        unit_id: str,
        num_channels: int,
        firmware_version: str,
        type: str,
        is_active: bool = True,
    ) -> Device:
        result = await self.db.execute(
            select(Device).where(Device.unit_id == unit_id)
        )
        device = result.scalar_one_or_none()

        if device is None:
            # create
            device = Device(
                unit_id=unit_id,
                num_channels=num_channels,
                firmware_version=firmware_version,
                type=type,
                is_active=is_active,
            )
            self.db.add(device)
            await self.db.commit()
            await self.db.refresh(device)
        else:
            # update
            device.num_channels = num_channels
            device.firmware_version = firmware_version
            device.type = type
            device.is_active = is_active

            await self.db.commit()
            await self.db.refresh(device)

        # sync channels after registration/update
        channel_repo = ChannelRepository(self.db)
        await channel_repo.register_or_update(device.id, num_channels, type)
        return device

    async def get_all_unit_ids(self) -> list[str]:
        """Return list of all unit_id from devices table."""
        result = await self.db.execute(select(Device.unit_id))
        return [row[0] for row in result.fetchall()]
    
    async def update(self, device_id: int, changes: dict) -> Device | None:
        result = await self.db.execute(
            select(Device)
            .options(selectinload(Device.channels))  # preload
            .where(Device.id == device_id)
        )
        device = result.scalar_one_or_none()
        if not device:
            return None

        for k, v in changes.items():
            setattr(device, k, v)

        await self.db.commit()
        await self.db.refresh(device)
        return device
