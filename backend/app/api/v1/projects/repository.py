from __future__ import annotations

from typing import Optional
from uuid import UUID

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.project import Project


class ProjectRepository:
    """CRUD helpers for workspace projects."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def list(self) -> list[Project]:
        stmt = select(Project).order_by(Project.created_at.asc())
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def get(self, project_id: int) -> Optional[Project]:
        stmt = select(Project).where(Project.id == project_id)
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def get_by_uuid(self, project_uuid: UUID) -> Optional[Project]:
        stmt = select(Project).where(Project.uuid == str(project_uuid))
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def create(self, data: dict) -> Project:
        project = Project(**data)
        self.db.add(project)
        await self.db.commit()
        await self.db.refresh(project)
        return project

    async def update(self, project_id: int, changes: dict) -> Optional[Project]:
        project = await self.get(project_id)
        if not project:
            return None
        for key, value in changes.items():
            setattr(project, key, value)
        await self.db.commit()
        await self.db.refresh(project)
        return project

    async def delete(self, project_id: int) -> bool:
        project = await self.get(project_id)
        if not project:
            return False
        await self.db.delete(project)
        await self.db.commit()
        return True
