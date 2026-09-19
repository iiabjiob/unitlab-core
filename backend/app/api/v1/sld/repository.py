from __future__ import annotations

from dataclasses import dataclass

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.workspace import Workspace
from app.models.workspace_sld import WorkspaceSldDocument, WorkspaceSldDocumentRevision
from app.schemas.sld_schema import SLD_DOCUMENT_SCHEMA, SldDocumentUpdateSchema


class SldWorkspaceNotFoundError(Exception):
    pass


@dataclass
class SldRevisionConflictError(Exception):
    current_revision: int


class WorkspaceSldRepository:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def get(self, workspace_id: int) -> WorkspaceSldDocument | None:
        result = await self.db.execute(
            select(WorkspaceSldDocument).where(WorkspaceSldDocument.workspace_id == workspace_id)
        )
        return result.scalar_one_or_none()

    async def save(self, workspace_id: int, payload: SldDocumentUpdateSchema) -> WorkspaceSldDocument:
        workspace = await self.db.scalar(select(Workspace).where(Workspace.id == workspace_id))
        if workspace is None:
            raise SldWorkspaceNotFoundError

        current = await self.db.scalar(
            select(WorkspaceSldDocument)
            .where(WorkspaceSldDocument.workspace_id == workspace_id)
            .with_for_update()
        )
        if current is None:
            if payload.base_revision != 0:
                raise SldRevisionConflictError(0)
            current = WorkspaceSldDocument(
                workspace_id=workspace_id,
                document_schema=payload.document_schema,
                revision=1,
                document=payload.document,
            )
            self.db.add(current)
            await self.db.flush()
        else:
            if payload.base_revision != current.revision:
                raise SldRevisionConflictError(current.revision)
            current.revision += 1
            current.document_schema = payload.document_schema
            current.document = payload.document

        self.db.add(
            WorkspaceSldDocumentRevision(
                document_id=current.id,
                revision=current.revision,
                document_schema=current.document_schema or SLD_DOCUMENT_SCHEMA,
                document=payload.document,
                change_kind=payload.change_kind,
            )
        )
        await self.db.commit()
        await self.db.refresh(current)
        return current
