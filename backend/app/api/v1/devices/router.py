from fastapi import APIRouter, Body, Depends, HTTPException, Query
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.services.device_service import DeviceService
from app.services.channel_service import ChannelService
from app.schemas.device_schema import (
    DeviceSchema,
    DeviceSummary,
    DeviceUpdate,
    DeviceBulkDeletePayload,
    DeviceBulkDeleteResult,
)
from app.schemas.channel_schema import ChannelListResponse


router = APIRouter(prefix="/api/v1/devices", tags=["Devices"])


# --------------------------
# Dependency: Device service
# --------------------------
def get_device_service(db: AsyncSession = Depends(get_db)) -> DeviceService:
    return DeviceService(db)


def get_channel_service(db: AsyncSession = Depends(get_db)) -> ChannelService:
    return ChannelService(db)


# --------------------------
# Devices
# --------------------------
@router.get("", response_model=list[DeviceSummary])
async def get_devices(service: DeviceService = Depends(get_device_service)):
    return await service.list()


@router.get("/{id}", response_model=DeviceSchema)
async def get_device(id: int, service: DeviceService = Depends(get_device_service)):
    dev = await service.get(id)
    if not dev:
        raise HTTPException(404, "Device not found")
    return dev


@router.patch("/{id}", response_model=DeviceSchema)
async def update_device(
    id: int,
    patch: DeviceUpdate,
    service: DeviceService = Depends(get_device_service),
):
    dev = await service.update(id, patch.model_dump(exclude_unset=True))
    if not dev:
        raise HTTPException(404, "Device not found")
    return dev


@router.delete("/{id}")
async def delete_device(id: int, service: DeviceService = Depends(get_device_service)):
    deleted = await service.delete(id)
    if not deleted:
        raise HTTPException(404, "Device not found")
    return {"detail": "Device deleted"}


# --------------------------
# Channels of a device
# --------------------------
@router.get("/{id}/channels", response_model=ChannelListResponse)
async def get_device_channels(
    id: int,
    limit: int = Query(default=100, ge=1, le=1000),
    offset: int = Query(default=0, ge=0),
    device_service: DeviceService = Depends(get_device_service),
    channel_service: ChannelService = Depends(get_channel_service),
):
    # Verify the device exists
    exists = await device_service.get(id)
    if not exists:
        raise HTTPException(404, "Device not found")

    return await channel_service.list_by_device_paginated(id, limit, offset)


# --------------------------
# Bulk delete
# --------------------------
@router.delete("/bulk", response_model=DeviceBulkDeleteResult)
async def bulk_delete_devices(
    payload: DeviceBulkDeletePayload = Body(...),
    service: DeviceService = Depends(get_device_service),
):
    # Always expect explicit body payload, no magic parsing
    if not payload.ids:
        raise HTTPException(400, "No device IDs provided")

    deleted = await service.delete_many(payload.ids)
    return DeviceBulkDeleteResult(deleted=deleted)
