from __future__ import annotations

import re
from typing import Optional

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.workspace import Workspace
from app.services.system_sequence_seeder import ensure_default_sequences_for_workspace

_SLUG_PATTERN = re.compile(r"[^a-z0-9]+")


def _slugify(value: str) -> str:
    slug = _SLUG_PATTERN.sub("-", value.lower()).strip("-")
    return slug or "workspace"


class WorkspaceRepository:
    """CRUD helpers for workspace entities plus slug normalization."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def list(self) -> list[Workspace]:
        stmt = select(Workspace).order_by(Workspace.created_at.asc())
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def get(self, workspace_id: int) -> Optional[Workspace]:
        stmt = select(Workspace).where(Workspace.id == workspace_id)
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def get_by_slug(self, slug: str) -> Optional[Workspace]:
        stmt = select(Workspace).where(Workspace.slug == slug)
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def create(self, data: dict) -> Workspace:
        payload = data.copy()
        payload["slug"] = await self._resolve_slug(payload.get("slug"), payload.get("name"))
        workspace = Workspace(**payload)
        self.db.add(workspace)
        await self.db.flush()
        await ensure_default_sequences_for_workspace(workspace.id, session=self.db)
        await self.db.commit()
        await self.db.refresh(workspace)
        return workspace

    async def update(self, workspace_id: int, changes: dict) -> Optional[Workspace]:
        workspace = await self.get(workspace_id)
        if not workspace:
            return None

        if "slug" in changes:
            changes["slug"] = await self._resolve_slug(
                changes.get("slug"),
                changes.get("name", workspace.name),
                exclude_id=workspace_id,
            )

        for key, value in changes.items():
            if value is None:
                continue
            setattr(workspace, key, value)

        await self.db.commit()
        await self.db.refresh(workspace)
        return workspace

    async def delete(self, workspace_id: int) -> bool:
        workspace = await self.get(workspace_id)
        if not workspace:
            return False
        await self.db.delete(workspace)
        await self.db.commit()
        return True

    async def _resolve_slug(
        self,
        candidate: Optional[str],
        fallback_name: Optional[str],
        *,
        exclude_id: Optional[int] = None,
    ) -> str:
        base = candidate or fallback_name or "workspace"
        slug = _slugify(base)
        if not await self._slug_in_use(slug, exclude_id):
            return slug

        suffix = 2
        while True:
            next_slug = f"{slug}-{suffix}"
            if not await self._slug_in_use(next_slug, exclude_id):
                return next_slug
            suffix += 1

    async def _slug_in_use(self, slug: str, exclude_id: Optional[int] = None) -> bool:
        stmt = select(Workspace.id).where(Workspace.slug == slug)
        if exclude_id is not None:
            stmt = stmt.where(Workspace.id != exclude_id)
        result = await self.db.execute(stmt.limit(1))
        return result.scalar_one_or_none() is not None
