from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from sqlalchemy import DateTime, ForeignKey, Index, String, Text
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - import for annotations only
    from app.models.sequence_run import SequenceRun
    from .sequence_step import SequenceStep
    from app.models.project import Project


class Sequence(Base):
    __tablename__ = "sequences"
    __table_args__ = (Index("ix_sequences_project_name", "project_id", "name"),)

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    project_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("projects.id", ondelete="CASCADE"), nullable=False
    )
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
    project: Mapped["Project"] = relationship(
        "Project", back_populates="sequences", lazy="selectin"
    )
