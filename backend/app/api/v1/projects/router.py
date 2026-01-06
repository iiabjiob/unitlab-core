from __future__ import annotations

from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.projects import ProjectRepository
from app.infrastructure.db.database import get_db
from app.schemas.project_schema import (
    ProjectCreateSchema,
    ProjectSchema,
    ProjectUpdateSchema,
)

router = APIRouter(prefix="/api/v1/projects", tags=["Projects"])


def get_repository(db: AsyncSession = Depends(get_db)) -> ProjectRepository:
    return ProjectRepository(db)


@router.get("", response_model=list[ProjectSchema])
async def list_projects(repo: ProjectRepository = Depends(get_repository)):
    return await repo.list()


@router.get("/{project_id}", response_model=ProjectSchema)
async def get_project(project_id: int, repo: ProjectRepository = Depends(get_repository)):
    project = await repo.get(project_id)
    if not project:
        raise HTTPException(status_code=404, detail="Project not found")
    return project


@router.get("/uuid/{project_uuid}", response_model=ProjectSchema)
async def get_project_by_uuid(
    project_uuid: UUID,
    repo: ProjectRepository = Depends(get_repository),
):
    project = await repo.get_by_uuid(project_uuid)
    if not project:
        raise HTTPException(status_code=404, detail="Project not found")
    return project


@router.post("", response_model=ProjectSchema)
async def create_project(
    payload: ProjectCreateSchema,
    repo: ProjectRepository = Depends(get_repository),
):
    return await repo.create(payload.model_dump())


@router.patch("/{project_id}", response_model=ProjectSchema)
async def update_project(
    project_id: int,
    payload: ProjectUpdateSchema,
    repo: ProjectRepository = Depends(get_repository),
):
    project = await repo.update(project_id, payload.model_dump(exclude_unset=True))
    if not project:
        raise HTTPException(status_code=404, detail="Project not found")
    return project


@router.delete("/{project_id}")
async def delete_project(
    project_id: int,
    repo: ProjectRepository = Depends(get_repository),
):
    deleted = await repo.delete(project_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Project not found")
    return {"detail": "Project deleted"}
