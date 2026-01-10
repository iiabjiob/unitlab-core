"""Workspace attachment helpers for switchgears and sequences."""
from __future__ import annotations

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.sequence import Sequence
from app.models.switchgear import Switchgear
from app.models.workspace import Workspace, WorkspaceSequence, WorkspaceSwitchgear


class WorkspaceEntityNotFoundError(Exception):
    """Raised when a workspace or asset cannot be found."""


class WorkspaceLinkNotFoundError(Exception):
    """Raised when an attach/detach operation targets a missing link."""


class WorkspaceLinksService:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def attach_switchgear(self, workspace_id: int, switchgear_id: int) -> None:
        await self._ensure_workspace(workspace_id)
        await self._ensure_switchgear(switchgear_id)
        stmt = select(WorkspaceSwitchgear).where(
            WorkspaceSwitchgear.workspace_id == workspace_id,
            WorkspaceSwitchgear.switchgear_id == switchgear_id,
        )
        if (await self.db.execute(stmt)).scalar_one_or_none():
            return
        self.db.add(WorkspaceSwitchgear(workspace_id=workspace_id, switchgear_id=switchgear_id))
        await self.db.commit()

    async def detach_switchgear(self, workspace_id: int, switchgear_id: int) -> None:
        stmt = select(WorkspaceSwitchgear).where(
            WorkspaceSwitchgear.workspace_id == workspace_id,
            WorkspaceSwitchgear.switchgear_id == switchgear_id,
        )
        link = (await self.db.execute(stmt)).scalar_one_or_none()
        if not link:
            raise WorkspaceLinkNotFoundError("Switchgear is not attached to this workspace")
        await self.db.delete(link)
        await self.db.commit()

    async def attach_sequence(self, workspace_id: int, sequence_id: int) -> None:
        await self._ensure_workspace(workspace_id)
        await self._ensure_sequence(sequence_id)
        stmt = select(WorkspaceSequence).where(
            WorkspaceSequence.workspace_id == workspace_id,
            WorkspaceSequence.sequence_id == sequence_id,
        )
        if (await self.db.execute(stmt)).scalar_one_or_none():
            return
        self.db.add(WorkspaceSequence(workspace_id=workspace_id, sequence_id=sequence_id))
        await self.db.commit()

    async def detach_sequence(self, workspace_id: int, sequence_id: int) -> None:
        stmt = select(WorkspaceSequence).where(
            WorkspaceSequence.workspace_id == workspace_id,
            WorkspaceSequence.sequence_id == sequence_id,
        )
        link = (await self.db.execute(stmt)).scalar_one_or_none()
        if not link:
            raise WorkspaceLinkNotFoundError("Sequence is not attached to this workspace")
        await self.db.delete(link)
        await self.db.commit()

    async def _ensure_workspace(self, workspace_id: int) -> None:
        stmt = select(Workspace.id).where(Workspace.id == workspace_id)
        if (await self.db.execute(stmt)).scalar_one_or_none() is None:
            raise WorkspaceEntityNotFoundError("Workspace not found")

    async def _ensure_switchgear(self, switchgear_id: int) -> None:
        stmt = select(Switchgear.id).where(Switchgear.id == switchgear_id)
        if (await self.db.execute(stmt)).scalar_one_or_none() is None:
            raise WorkspaceEntityNotFoundError("Switchgear not found")

    async def _ensure_sequence(self, sequence_id: int) -> None:
        stmt = select(Sequence.id).where(Sequence.id == sequence_id)
        if (await self.db.execute(stmt)).scalar_one_or_none() is None:
            raise WorkspaceEntityNotFoundError("Sequence not found")
