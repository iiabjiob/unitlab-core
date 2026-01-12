"""Signal snapshot and allocation persistence models."""
from __future__ import annotations

from datetime import datetime
from enum import Enum
from typing import TYPE_CHECKING, Any

from sqlalchemy import (
    DateTime,
    Enum as SAEnum,
    ForeignKey,
    Index,
    Integer,
    String,
    UniqueConstraint,
)
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - import for annotations only
    from app.models.test_run import TestRun
    from app.models.workspace import Workspace


class SignalSnapshotStatus(str, Enum):
    """Lifecycle marker for signal snapshots."""

    DRAFT = "draft"
    LOCKED = "locked"


class SignalSnapshot(Base):
    __tablename__ = "signal_snapshots"
    __table_args__ = (
        Index("ix_signal_snapshots_workspace", "workspace_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    status: Mapped[SignalSnapshotStatus] = mapped_column(
        SAEnum(
            SignalSnapshotStatus,
            name="signal_snapshot_status_enum",
            values_callable=lambda enum_cls: [member.value for member in enum_cls],
            validate_strings=True,
        ),
        nullable=False,
        server_default=SignalSnapshotStatus.DRAFT.value,
    )
    source_filename: Mapped[str | None] = mapped_column(String(255), nullable=True)
    source_hash: Mapped[str | None] = mapped_column(String(64), nullable=True)
    rows_count: Mapped[int] = mapped_column(Integer, nullable=False)
    schema_version: Mapped[int] = mapped_column(Integer, nullable=False, server_default="1")
    data: Mapped[dict[str, Any]] = mapped_column(JSONB, nullable=False)
    locked_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship(
        "Workspace", back_populates="signal_snapshots", lazy="selectin"
    )
    test_runs: Mapped[list["TestRun"]] = relationship(
        "TestRun",
        back_populates="signal_snapshot",
        lazy="selectin",
    )

    def is_locked(self) -> bool:
        return self.status == SignalSnapshotStatus.LOCKED