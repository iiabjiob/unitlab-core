from __future__ import annotations

from datetime import datetime

from sqlalchemy import DateTime, ForeignKey, Index, Integer, JSON, LargeBinary, String, UniqueConstraint, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK


class WorkspaceIec61850SclImport(Base):
    __tablename__ = "workspace_iec61850_scl_imports"
    __table_args__ = (
        UniqueConstraint("workspace_id", "source_hash", "selected_ied", name="uq_workspace_iec61850_scl_import_hash_ied"),
        Index("ix_workspace_iec61850_scl_imports_workspace_created", "workspace_id", "created_at", "id"),
        Index("ix_workspace_iec61850_scl_imports_hash", "source_hash"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    source_filename: Mapped[str | None] = mapped_column(String(255), nullable=True)
    source_hash: Mapped[str] = mapped_column(String(64), nullable=False)
    source_size: Mapped[int] = mapped_column(BIGINT_PK, nullable=False)
    selected_ied: Mapped[str] = mapped_column(String(128), nullable=False)
    normalized_schema: Mapped[str] = mapped_column(String(128), nullable=False)
    source_bytes: Mapped[bytes] = mapped_column(LargeBinary, nullable=False)
    normalized_model: Mapped[dict] = mapped_column(JSON, nullable=False, server_default="{}")
    diagnostics: Mapped[list] = mapped_column(JSON, nullable=False, server_default="[]")
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    workspace = relationship("Workspace", lazy="selectin")


class WorkspaceIec61850RuntimeSelection(Base):
    __tablename__ = "workspace_iec61850_runtime_selections"
    __table_args__ = (
        UniqueConstraint("workspace_id", name="uq_w_iec61850_rt_sel_workspace"),
        Index("ix_w_iec61850_rt_sel_import", "scl_import_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False
    )
    scl_import_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("workspace_iec61850_scl_imports.id", ondelete="RESTRICT"), nullable=False
    )
    runtime_revision: Mapped[int] = mapped_column(Integer, nullable=False, server_default="1")
    selected_by: Mapped[str | None] = mapped_column(String(128), nullable=True)
    selection_reason: Mapped[str | None] = mapped_column(String(255), nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    workspace = relationship("Workspace", lazy="selectin")
    scl_import = relationship("WorkspaceIec61850SclImport", lazy="selectin")


class WorkspaceIec61850RuntimeSelectionEvent(Base):
    __tablename__ = "workspace_iec61850_runtime_selection_events"
    __table_args__ = (
        Index("ix_w_iec61850_rt_sel_events_ws_created", "workspace_id", "created_at", "id"),
        Index("ix_w_iec61850_rt_sel_events_import", "scl_import_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False
    )
    scl_import_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("workspace_iec61850_scl_imports.id", ondelete="RESTRICT"), nullable=False
    )
    runtime_revision: Mapped[int] = mapped_column(Integer, nullable=False)
    operation: Mapped[str] = mapped_column(String(32), nullable=False)
    selected_by: Mapped[str | None] = mapped_column(String(128), nullable=True)
    selection_reason: Mapped[str | None] = mapped_column(String(255), nullable=True)
    payload: Mapped[dict] = mapped_column(JSON, nullable=False, server_default="{}")
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    workspace = relationship("Workspace", lazy="selectin")
    scl_import = relationship("WorkspaceIec61850SclImport", lazy="selectin")
