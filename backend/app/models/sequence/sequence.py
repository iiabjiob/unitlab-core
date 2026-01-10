from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from sqlalchemy import DateTime, Index, String, Text
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - import for annotations only
    from app.models.sequence_run import SequenceRun
    from .sequence_step import SequenceStep
    from app.models.workspace import Workspace, WorkspaceSequence


class Sequence(Base):
    __tablename__ = "sequences"
    __table_args__: tuple = ()

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String, nullable=False)
    description: Mapped[str | None] = mapped_column(Text, nullable=True)
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    steps: Mapped[list["SequenceStep"]] = relationship(
        "SequenceStep",
        back_populates="sequence",
        cascade="all, delete-orphan",
        order_by="SequenceStep.order_index",
        lazy="selectin",
    )

    runs: Mapped[list["SequenceRun"]] = relationship(
        "SequenceRun",
        back_populates="sequence",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    workspace_links: Mapped[list["WorkspaceSequence"]] = relationship(
        "WorkspaceSequence",
        back_populates="sequence",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    workspaces: Mapped[list["Workspace"]] = relationship(
        "Workspace",
        secondary="workspace_sequences",
        viewonly=True,
        lazy="selectin",
    )
