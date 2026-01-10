from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.workspaces import WorkspaceRepository
from app.infrastructure.db.database import get_db
from app.schemas.workspace_schema import (
    WorkspaceCreateSchema,
    WorkspaceSchema,
    WorkspaceUpdateSchema,
)
from app.services.workspace_links_service import (
    WorkspaceEntityNotFoundError,
    WorkspaceLinkNotFoundError,
    WorkspaceLinksService,
)

router = APIRouter(prefix="/api/v1/workspaces", tags=["Workspaces"])


def get_repository(db: AsyncSession = Depends(get_db)) -> WorkspaceRepository:
    return WorkspaceRepository(db)


def get_links_service(db: AsyncSession = Depends(get_db)) -> WorkspaceLinksService:
    return WorkspaceLinksService(db)


@router.get("", response_model=list[WorkspaceSchema])
async def list_workspaces(repo: WorkspaceRepository = Depends(get_repository)):
    return await repo.list()


@router.get("/{workspace_id}", response_model=WorkspaceSchema)
async def get_workspace(workspace_id: int, repo: WorkspaceRepository = Depends(get_repository)):
    workspace = await repo.get(workspace_id)
    if not workspace:
        raise HTTPException(status_code=404, detail="Workspace not found")
    return workspace


@router.get("/slug/{slug}", response_model=WorkspaceSchema)
async def get_workspace_by_slug(slug: str, repo: WorkspaceRepository = Depends(get_repository)):
    workspace = await repo.get_by_slug(slug)
    if not workspace:
        raise HTTPException(status_code=404, detail="Workspace not found")
    return workspace


@router.post("", response_model=WorkspaceSchema)
async def create_workspace(
    payload: WorkspaceCreateSchema,
    repo: WorkspaceRepository = Depends(get_repository),
):
    return await repo.create(payload.model_dump())


@router.patch("/{workspace_id}", response_model=WorkspaceSchema)
async def update_workspace(
    workspace_id: int,
    payload: WorkspaceUpdateSchema,
    repo: WorkspaceRepository = Depends(get_repository),
):
    workspace = await repo.update(workspace_id, payload.model_dump(exclude_unset=True))
    if not workspace:
        raise HTTPException(status_code=404, detail="Workspace not found")
    return workspace


@router.delete("/{workspace_id}")
async def delete_workspace(
    workspace_id: int,
    repo: WorkspaceRepository = Depends(get_repository),
):
    deleted = await repo.delete(workspace_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Workspace not found")
    return {"detail": "Workspace deleted"}


@router.post("/{workspace_id}/switchgears/{switchgear_id}")
async def attach_switchgear(
    workspace_id: int,
    switchgear_id: int,
    service: WorkspaceLinksService = Depends(get_links_service),
):
    try:
        await service.attach_switchgear(workspace_id, switchgear_id)
    except WorkspaceEntityNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc))
    return {"detail": "Switchgear attached"}


@router.delete("/{workspace_id}/switchgears/{switchgear_id}")
async def detach_switchgear(
    workspace_id: int,
    switchgear_id: int,
    service: WorkspaceLinksService = Depends(get_links_service),
):
    try:
        await service.detach_switchgear(workspace_id, switchgear_id)
    except WorkspaceLinkNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc))
    except WorkspaceEntityNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc))
    return {"detail": "Switchgear detached"}


@router.post("/{workspace_id}/sequences/{sequence_id}")
async def attach_sequence(
    workspace_id: int,
    sequence_id: int,
    service: WorkspaceLinksService = Depends(get_links_service),
):
    try:
        await service.attach_sequence(workspace_id, sequence_id)
    except WorkspaceEntityNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc))
    return {"detail": "Sequence attached"}


@router.delete("/{workspace_id}/sequences/{sequence_id}")
async def detach_sequence(
    workspace_id: int,
    sequence_id: int,
    service: WorkspaceLinksService = Depends(get_links_service),
):
    try:
        await service.detach_sequence(workspace_id, sequence_id)
    except WorkspaceLinkNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc))
    except WorkspaceEntityNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc))
    return {"detail": "Sequence detached"}
