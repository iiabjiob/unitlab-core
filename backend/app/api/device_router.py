# app/api/device_router.py
from fastapi import APIRouter, Depends, Query, HTTPException
from typing import Optional
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.repositories.device_repository import DeviceRepository
from app.schemas.device_schema import DeviceSchema, DeviceUpdateSchema
from app.schemas.channel_schema import ChannelSchema
from app.core.utils import to_str
from app.infrastructure.redis.manager import RedisManager

router = APIRouter(prefix="/api/devices", tags=["Devices"])


@router.get("", response_model=list[DeviceSchema])
async def get_devices(
    db: AsyncSession = Depends(get_db),
    is_active: Optional[bool] = Query(None),
    type_: Optional[str] = Query(None),
):
    repo = DeviceRepository(db)
    devices = await repo.get_all(is_active=is_active, type_=type_)

    redis_client = RedisManager.get_instance()

    enriched: list[DeviceSchema] = []
    for dev in devices:
        status = await redis_client.get(f"device:{dev.unit_id}:status")
        last_seen = await redis_client.get(f"device:{dev.unit_id}:last_seen")

        schema = DeviceSchema.model_validate(dev)
        schema.status = to_str(status, "offline")
        schema.last_seen = int(last_seen) if last_seen else None
        schema.channels = [ChannelSchema.model_validate(ch) for ch in dev.channels]
        enriched.append(schema)

    return enriched


@router.get("/{id}", response_model=DeviceSchema)
async def get_device(id: int, db: AsyncSession = Depends(get_db)):
    repo = DeviceRepository(db)
    device = await repo.get(id)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    redis_client = RedisManager.get_instance()
    status = await redis_client.get(f"device:{device.unit_id}:status")
    last_seen = await redis_client.get(f"device:{device.unit_id}:last_seen")

    schema = DeviceSchema.model_validate(device)
    schema.status = to_str(status, "offline")
    schema.last_seen = int(last_seen) if last_seen else None
    schema.channels = [ChannelSchema.model_validate(ch) for ch in device.channels]
    return schema


@router.patch("/{id}", response_model=DeviceSchema)
async def update_device(
    id: int,
    patch: DeviceUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = DeviceRepository(db)
    device = await repo.update(id, patch.model_dump(exclude_unset=True))
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    redis_client = RedisManager.get_instance()
    status = await redis_client.get(f"device:{device.unit_id}:status")
    last_seen = await redis_client.get(f"device:{device.unit_id}:last_seen")

    schema = DeviceSchema.model_validate(device)
    schema.status = to_str(status, "offline")
    schema.last_seen = int(last_seen) if last_seen else None
    return schema


@router.delete("/{id}", summary="Delete device")
async def delete_device(id: int, db: AsyncSession = Depends(get_db)):
    repo = DeviceRepository(db)
    deleted = await repo.delete(id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Device not found")
    return {"detail": "Device deleted"}
