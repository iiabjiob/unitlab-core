from __future__ import annotations

from datetime import datetime

from sqlalchemy import DateTime, ForeignKey, Index, Integer, JSON, String, UniqueConstraint, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK


class WorkspaceSldDocument(Base):
    __tablename__ = "workspace_sld_documents"
    __table_args__ = (UniqueConstraint("workspace_id", name="uq_workspace_sld_documents_workspace"),)

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False
    )
    document_schema: Mapped[str] = mapped_column(String(64), nullable=False, server_default="unitlab.sld.v1")
    revision: Mapped[int] = mapped_column(Integer, nullable=False, server_default="0")
    document: Mapped[dict[str, object]] = mapped_column(JSON, nullable=False, server_default="{}")
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now(), nullable=False)
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    revisions: Mapped[list[WorkspaceSldDocumentRevision]] = relationship(
        "WorkspaceSldDocumentRevision", back_populates="document_record", cascade="all, delete-orphan"
    )


class WorkspaceSldDocumentRevision(Base):
    __tablename__ = "workspace_sld_document_revisions"
    __table_args__ = (
        UniqueConstraint("document_id", "revision", name="uq_workspace_sld_document_revisions_revision"),
        Index("ix_workspace_sld_document_revisions_document_created", "document_id", "created_at", "id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    document_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("workspace_sld_documents.id", ondelete="CASCADE"), nullable=False
    )
    revision: Mapped[int] = mapped_column(Integer, nullable=False)
    document_schema: Mapped[str] = mapped_column(String(64), nullable=False)
    document: Mapped[dict[str, object]] = mapped_column(JSON, nullable=False)
    change_kind: Mapped[str] = mapped_column(String(32), nullable=False, server_default="edit")
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now(), nullable=False)

    document_record: Mapped[WorkspaceSldDocument] = relationship(
        "WorkspaceSldDocument", back_populates="revisions"
    )
