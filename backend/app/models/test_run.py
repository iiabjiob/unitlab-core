"""Test run persistence models."""
from __future__ import annotations

from datetime import datetime
from enum import Enum
from typing import TYPE_CHECKING, Any

from sqlalchemy import (
    DateTime,
    Enum as SAEnum,
    ForeignKey,
    Index,
)
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - import for annotations only
    from app.models.sequence import Sequence
    from app.models.signal_snapshot import SignalSnapshot
    from app.models.workspace import Workspace


class TestRunStatus(str, Enum):
    """Lifecycle of a test run."""

    CREATED = "created"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"


class TestRun(Base):
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
    sequence_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("sequences.id", ondelete="RESTRICT"),
        nullable=False,
    )
    signal_snapshot_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("signal_snapshots.id", ondelete="RESTRICT"),
        nullable=False,
    )
    allocation_snapshot: Mapped[list[dict[str, Any]]] = mapped_column(JSONB, nullable=False)
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
    sequence: Mapped["Sequence"] = relationship(
        "Sequence", back_populates="test_runs", lazy="selectin"
    )
    signal_snapshot: Mapped["SignalSnapshot"] = relationship(
        "SignalSnapshot", back_populates="test_runs", lazy="selectin"
    )
