"""Test run persistence models."""
from __future__ import annotations

from datetime import datetime
from enum import Enum
from typing import TYPE_CHECKING, Any

from sqlalchemy import (
    CheckConstraint,
    DateTime,
    Enum as SAEnum,
    ForeignKey,
    Index,
    UniqueConstraint,
)
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - import for annotations only
    from app.models.allocation import Allocation
    from app.models.sequence import Sequence
    from app.models.test_run_signal_snapshot import TestRunSignalSnapshot
    from app.models.workspace import Workspace


class TestRunStatus(str, Enum):
    """Lifecycle of a test run."""

    CREATED = "created"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"


class TestRun(Base):
    """Execution context tying allocation + sequences to frozen signal snapshots."""

    __tablename__ = "test_runs"
    __table_args__ = (
        Index("ix_test_runs_workspace_created", "workspace_id", "created_at"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    status: Mapped[TestRunStatus] = mapped_column(
        SAEnum(TestRunStatus, name="test_run_status_enum"),
        nullable=False,
        server_default=TestRunStatus.CREATED.value,
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    started_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    finished_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    execution_meta: Mapped[dict[str, Any] | None] = mapped_column(JSONB, nullable=True)

    workspace: Mapped["Workspace"] = relationship(
        "Workspace", back_populates="test_runs", lazy="selectin"
    )
    snapshot: Mapped["TestRunSignalSnapshot"] = relationship(
        "TestRunSignalSnapshot",
        back_populates="test_run",
        cascade="all, delete-orphan",
        uselist=False,
        lazy="selectin",
    )
    allocation: Mapped["Allocation"] = relationship(
        "Allocation",
        back_populates="test_run",
        cascade="all, delete-orphan",
        uselist=False,
        lazy="selectin",
    )
    sequence_links: Mapped[list["TestRunSequenceLink"]] = relationship(
        "TestRunSequenceLink",
        back_populates="test_run",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    sequences: Mapped[list["Sequence"]] = relationship(
        "Sequence",
        secondary="test_run_sequences",
        back_populates="test_runs",
        lazy="selectin",
    )

    @property
    def sequence_ids(self) -> list[int]:  # pragma: no cover - convenience for serializers
        return [link.sequence_id for link in self.sequence_links]


class TestRunSequenceLink(Base):
    """Association between a TestRun and the sequences executed within it."""

    __tablename__ = "test_run_sequences"
    __table_args__ = (
        UniqueConstraint("test_run_id", "sequence_id", name="uq_test_run_sequence_pair"),
        Index("ix_test_run_sequences_test_run", "test_run_id"),
        Index("ix_test_run_sequences_sequence", "sequence_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    test_run_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("test_runs.id", ondelete="CASCADE"),
        nullable=False,
    )
    sequence_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("sequences.id", ondelete="RESTRICT"),
        nullable=False,
    )

    test_run: Mapped["TestRun"] = relationship("TestRun", back_populates="sequence_links")
    sequence: Mapped["Sequence"] = relationship("Sequence", back_populates="test_run_links")
