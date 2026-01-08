from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.schemas.datapoint_schema import (
    DatapointCreate,
    DatapointListResponse,
    DatapointSchema,
    DatapointUpdate,
)
from app.schemas.allocation_schema import AllocationAssignRequest, AllocationSchema
from app.services.datapoint_service import DatapointService
from app.services.allocation_service import (
    AllocationConflictError,
    AllocationService,
    AllocationError,
    ChannelNotFoundError,
    DatapointNotFoundError,
)

router = APIRouter(prefix="/api/v1/projects/{project_id}/datapoints", tags=["Datapoints"])


def get_datapoint_service(db: AsyncSession = Depends(get_db)) -> DatapointService:
    return DatapointService(db)


def get_allocation_service(db: AsyncSession = Depends(get_db)) -> AllocationService:
    return AllocationService(db)


@router.get("", response_model=DatapointListResponse)
async def list_datapoints(
    project_id: int,
    limit: int = Query(default=100, ge=1, le=1000),
    offset: int = Query(default=0, ge=0),
    service: DatapointService = Depends(get_datapoint_service),
):
    return await service.list_paginated(project_id, limit, offset)


@router.get("/{datapoint_id}", response_model=DatapointSchema)
async def get_datapoint(
    project_id: int,
    datapoint_id: int,
    service: DatapointService = Depends(get_datapoint_service),
):
    datapoint = await service.get(project_id, datapoint_id)
    if not datapoint:
        raise HTTPException(status_code=404, detail="Datapoint not found")
    return datapoint


@router.post("", response_model=DatapointSchema, status_code=201)
async def create_datapoint(
    project_id: int,
    payload: DatapointCreate,
    service: DatapointService = Depends(get_datapoint_service),
):
    data = payload.model_dump()
    return await service.create(project_id, data)


@router.patch("/{datapoint_id}", response_model=DatapointSchema)
async def update_datapoint(
    project_id: int,
    datapoint_id: int,
    payload: DatapointUpdate,
    service: DatapointService = Depends(get_datapoint_service),
):
    datapoint = await service.update(project_id, datapoint_id, payload.model_dump(exclude_unset=True))
    if not datapoint:
        raise HTTPException(status_code=404, detail="Datapoint not found")
    return datapoint


@router.delete("/{datapoint_id}")
async def delete_datapoint(
    project_id: int,
    datapoint_id: int,
    service: DatapointService = Depends(get_datapoint_service),
):
    deleted = await service.delete(project_id, datapoint_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Datapoint not found")
    return {"detail": "Datapoint deleted"}


@router.get("/{datapoint_id}/allocation", response_model=AllocationSchema | None)
async def get_datapoint_allocation(
    project_id: int,
    datapoint_id: int,
    service: AllocationService = Depends(get_allocation_service),
):
    return await service.get_for_datapoint(project_id, datapoint_id)


@router.put("/{datapoint_id}/allocation", response_model=AllocationSchema)
async def assign_datapoint_allocation(
    project_id: int,
    datapoint_id: int,
    payload: AllocationAssignRequest,
    service: AllocationService = Depends(get_allocation_service),
):
    try:
        return await service.assign(project_id, datapoint_id, payload.channel_id)
    except DatapointNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    except ChannelNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    except AllocationConflictError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
    except AllocationError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc


@router.delete("/{datapoint_id}/allocation")
async def clear_datapoint_allocation(
    project_id: int,
    datapoint_id: int,
    service: AllocationService = Depends(get_allocation_service),
):
    removed = await service.clear(project_id, datapoint_id)
    if not removed:
        # Clearing an already empty allocation is idempotent
        return {"detail": "No allocation to clear"}
    return {"detail": "Allocation cleared"}
