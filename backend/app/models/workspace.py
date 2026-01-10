from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING
from uuid import uuid4

from sqlalchemy import DateTime, ForeignKey, Index, String, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover
    from app.models.switchgear import Switchgear
    from app.models.sequence import Sequence


class Workspace(Base):
    __tablename__ = "workspaces"

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    uuid: Mapped[str] = mapped_column(String(36), unique=True, nullable=False, default=lambda: str(uuid4()))
    name: Mapped[str] = mapped_column(String(255), nullable=False, index=True)
    slug: Mapped[str] = mapped_column(String(255), nullable=False, unique=True)

    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    switchgear_links: Mapped[list["WorkspaceSwitchgear"]] = relationship(
        "WorkspaceSwitchgear",
        back_populates="workspace",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    sequence_links: Mapped[list["WorkspaceSequence"]] = relationship(
        "WorkspaceSequence",
        back_populates="workspace",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    switchgears: Mapped[list["Switchgear"]] = relationship(
        "Switchgear",
        secondary="workspace_switchgears",
        viewonly=True,
        lazy="selectin",
    )
    sequences: Mapped[list["Sequence"]] = relationship(
        "Sequence",
        secondary="workspace_sequences",
        viewonly=True,
        lazy="selectin",
    )

    def __repr__(self) -> str:  # pragma: no cover - debug helper
        return f"Workspace(id={self.id!r}, slug={self.slug!r})"


class WorkspaceSwitchgear(Base):
    __tablename__ = "workspace_switchgears"
    __table_args__ = (
        UniqueConstraint("workspace_id", "switchgear_id", name="uq_workspace_switchgear"),
        Index("ix_workspace_switchgears_workspace", "workspace_id"),
        Index("ix_workspace_switchgears_switchgear", "switchgear_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False
    )
    switchgear_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("switchgears.id", ondelete="CASCADE"), nullable=False
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship("Workspace", back_populates="switchgear_links")
    switchgear: Mapped["Switchgear"] = relationship("Switchgear", back_populates="workspace_links")


class WorkspaceSequence(Base):
    __tablename__ = "workspace_sequences"
    __table_args__ = (
        UniqueConstraint("workspace_id", "sequence_id", name="uq_workspace_sequence"),
        Index("ix_workspace_sequences_workspace", "workspace_id"),
        Index("ix_workspace_sequences_sequence", "sequence_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False
    )
    sequence_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("sequences.id", ondelete="CASCADE"), nullable=False
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship("Workspace", back_populates="sequence_links")
    sequence: Mapped["Sequence"] = relationship("Sequence", back_populates="workspace_links")
