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

router = APIRouter(prefix="/api/v1/workspaces", tags=["Workspaces"])


def get_repository(db: AsyncSession = Depends(get_db)) -> WorkspaceRepository:
    return WorkspaceRepository(db)


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
