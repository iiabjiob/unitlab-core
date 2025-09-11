from fastapi import APIRouter, Depends, Query, HTTPException
from typing import Optional
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload
from app.infrastructure.db.database import get_db
from app.models.device import Device
from app.schemas.device_schema import DeviceSchema, DeviceUpdateSchema
from app.schemas.channel_schema import ChannelSchema
from app.core.utils import to_str
from app.infrastructure.redis.manager import RedisManager

router = APIRouter(prefix="/api", tags=["Devices"])

@router.get("/devices", response_model=list[DeviceSchema])
async def get_devices(
    db: AsyncSession = Depends(get_db),
    is_active: Optional[bool] = Query(None),
    type_: Optional[str] = Query(None),
):
    query = select(Device).options(selectinload(Device.channels))

    if is_active is not None:
        query = query.where(Device.is_active == is_active)
    if type_ is not None:
        query = query.where(Device.type == type_)

    result = await db.execute(query)
    devices = result.scalars().all()

    redis_client = RedisManager.get_instance()

    enriched: list[DeviceSchema] = []
    for dev in devices:
        # достаём статус из Redis
        status = await redis_client.get(f"device:{dev.unit_id}:status")
        last_seen = await redis_client.get(f"device:{dev.unit_id}:last_seen")

        schema = DeviceSchema.model_validate(dev)
        schema.status = to_str(status, "offline")
        schema.last_seen = int(last_seen) if last_seen else None
        schema.channels = [ChannelSchema.model_validate(ch) for ch in dev.channels]

        enriched.append(schema)

    return enriched

@router.get("/devices/{unit_id}", response_model=DeviceSchema)
async def get_device(unit_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(
        select(Device)
        .where(Device.unit_id == unit_id)
        .options(selectinload(Device.channels))
    )
    device = result.scalar_one_or_none()

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

@router.patch("/devices/{unit_id}", response_model=DeviceSchema)
async def update_device(
    unit_id: str,
    patch: DeviceUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    result = await db.execute(select(Device).where(Device.unit_id == unit_id))
    device = result.scalar_one_or_none()

    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    # обновляем только переданные поля
    for field, value in patch.dict(exclude_unset=True).items():
        setattr(device, field, value)

    await db.commit()
    await db.refresh(device)

    # enrich динамическими полями (status/last_seen)
    redis_client = RedisManager.get_instance()
    status = await redis_client.get(f"device:{device.unit_id}:status")
    last_seen = await redis_client.get(f"device:{device.unit_id}:last_seen")

    schema = DeviceSchema.model_validate(device)
    schema.status = to_str(status, "offline")
    schema.last_seen = int(last_seen) if last_seen else None
    return schema

@router.delete("/devices/{unit_id}", summary="Delete device")
async def delete_device(unit_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Device).where(Device.unit_id == unit_id))
    device = result.scalar_one_or_none()

    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    await db.delete(device)
    await db.commit()

    return {"detail": "Device deleted"}