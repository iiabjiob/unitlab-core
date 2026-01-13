"""Live Signal model representing editable FAT signals."""
from __future__ import annotations

from datetime import datetime
from enum import Enum
from typing import TYPE_CHECKING, Any

from sqlalchemy import (
    Boolean,
    DateTime,
    Enum as SAEnum,
    ForeignKey,
    Index,
    String,
    UniqueConstraint,
    text,
)
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - hints only
    from app.models.allocation import AllocationEntry
    from app.models.test_run_signal_snapshot import TestRunSignalSnapshotEntry
    from app.models.workspace import Workspace


class SignalIODirection(str, Enum):
    """Enumerates supported IO directions for signals."""

    DI = "DI"
    DO = "DO"
    AI = "AI"
    AO = "AO"


class Signal(Base):
    __tablename__ = "signals"
    __table_args__ = (
        UniqueConstraint("workspace_id", "key", name="uq_signals_workspace_key"),
        Index("ix_signals_workspace", "workspace_id"),
        Index("ix_signals_workspace_active", "workspace_id", "is_active"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    key: Mapped[str] = mapped_column(String(128), nullable=False)
    name: Mapped[str] = mapped_column(String(255), nullable=False)
    io_direction: Mapped[SignalIODirection] = mapped_column(
        SAEnum(SignalIODirection, name="signal_io_direction_enum"),
        nullable=False,
    )
    category: Mapped[str | None] = mapped_column(String(64), nullable=True)
    signal_metadata: Mapped[dict[str, Any]] = mapped_column(
        "metadata",
        JSONB,
        nullable=False,
        server_default=text("'{}'::jsonb"),
    )
    is_active: Mapped[bool] = mapped_column(Boolean, nullable=False, server_default=text("true"))
    deleted_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship(
        "Workspace",
        back_populates="signals",
        lazy="selectin",
    )
    allocation_entries: Mapped[list["AllocationEntry"]] = relationship(
        "AllocationEntry",
        back_populates="signal",
        lazy="selectin",
    )
    snapshot_entries: Mapped[list["TestRunSignalSnapshotEntry"]] = relationship(
        "TestRunSignalSnapshotEntry",
        back_populates="live_signal",
        lazy="selectin",
    )

    @property
    def is_deleted(self) -> bool:
        return self.deleted_at is not None

