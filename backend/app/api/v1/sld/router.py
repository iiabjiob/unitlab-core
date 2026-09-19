from __future__ import annotations

from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.sld.repository import (
    SldRevisionConflictError,
    SldWorkspaceNotFoundError,
    WorkspaceSldRepository,
)
from app.infrastructure.db.database import get_db
from app.schemas.sld_schema import SLD_DOCUMENT_SCHEMA, SldDocumentResponse, SldDocumentUpdateSchema

router = APIRouter(prefix="/api/v1/workspaces/{workspace_id}/sld", tags=["SLD"])


def get_repository(db: Annotated[AsyncSession, Depends(get_db)]) -> WorkspaceSldRepository:
    return WorkspaceSldRepository(db)


@router.get("", response_model=SldDocumentResponse)
async def get_sld_document(
    workspace_id: int,
    repo: Annotated[WorkspaceSldRepository, Depends(get_repository)],
):
    document = await repo.get(workspace_id)
    if document is None:
        return SldDocumentResponse(
            workspace_id=workspace_id,
            revision=0,
            document_schema=SLD_DOCUMENT_SCHEMA,
            document={},
            updated_at=None,
        )
    return document


@router.put("", response_model=SldDocumentResponse)
async def save_sld_document(
    workspace_id: int,
    payload: SldDocumentUpdateSchema,
    repo: Annotated[WorkspaceSldRepository, Depends(get_repository)],
):
    try:
        return await repo.save(workspace_id, payload)
    except SldWorkspaceNotFoundError:
        raise HTTPException(status_code=404, detail="Workspace not found")
    except SldRevisionConflictError as exc:
        raise HTTPException(
            status_code=409,
            detail=f"SLD document was changed by another user; current revision is {exc.current_revision}",
            headers={"X-SLD-Current-Revision": str(exc.current_revision)},
        )
