from fastapi import APIRouter, Depends, Query, HTTPException
from typing import Optional
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.infrastructure.db.database import get_db
from app.models.device import Device
from app.schemas.device_schema import DeviceSchema
from app.core.utils import to_str
from app.infrastructure.redis.manager import RedisManager

router = APIRouter(prefix="/api", tags=["Devices"])

@router.get("/devices", response_model=list[DeviceSchema])
async def get_devices(
    db: AsyncSession = Depends(get_db),
    is_active: Optional[bool] = Query(None),
    type_: Optional[str] = Query(None),
):
    query = select(Device)

    if is_active is not None:
        query = query.where(Device.is_active == is_active)
    if type_ is not None:
        query = query.where(Device.type_ == type_)

    result = await db.execute(query)
    devices = result.scalars().all()
    
    redis_client = RedisManager.get_instance()

    enriched: list[DeviceSchema] = []
    for dev in devices:
        # достаём статус из Redis
        status = await redis_client.get(f"device:{dev.unit_id}:status")
        last_seen = await redis_client.get(f"device:{dev.unit_id}:last_seen")
      
        enriched.append(DeviceSchema(
            unit_id=dev.unit_id,
            type=dev.type,
            channels=dev.channels,
            firmware_version=dev.firmware_version,
            is_active=dev.is_active,
            status = to_str(status, "offline"),
            last_seen=int(last_seen) if last_seen else None,
        ))

    return enriched

@router.patch("/devices/{unit_id}", summary="Toggle device is_active status")
async def toggle_device_is_active(unit_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Device).where(Device.unit_id == unit_id))
    device = result.scalar_one_or_none()

    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    device.is_active = not device.is_active
    await db.commit()
    await db.refresh(device)

    return {
        "unit_id": device.unit_id,
        "is_active": device.is_active
    }

@router.delete("/devices/{unit_id}", summary="Delete device")
async def delete_device(unit_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Device).where(Device.unit_id == unit_id))
    device = result.scalar_one_or_none()

    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    await db.delete(device)
    await db.commit()

    return {"detail": "Device deleted"}