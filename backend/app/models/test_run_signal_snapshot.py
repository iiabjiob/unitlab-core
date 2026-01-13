"""Immutable signal snapshot models owned by TestRuns."""
from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING, Any

from sqlalchemy import (
    DateTime,
    Enum as SAEnum,
    ForeignKey,
    Index,
    String,
    text,
)
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.signal import SignalIODirection
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing helpers only
    from app.models.channel import Channel
    from app.models.signal import Signal
    from app.models.test_run import TestRun
    from app.models.workspace import Workspace


class TestRunSignalSnapshot(Base):
    __tablename__ = "test_run_signal_snapshots"
    __table_args__ = (
        Index("ix_test_run_signal_snapshots_workspace", "workspace_id"),
    )

    test_run_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("test_runs.id", ondelete="CASCADE"),
        primary_key=True,
    )
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    captured_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    test_run: Mapped["TestRun"] = relationship(
        "TestRun",
        back_populates="snapshot",
        lazy="selectin",
    )
    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")
    entries: Mapped[list["TestRunSignalSnapshotEntry"]] = relationship(
        "TestRunSignalSnapshotEntry",
        back_populates="snapshot",
        cascade="all, delete-orphan",
        lazy="selectin",
    )


class TestRunSignalSnapshotEntry(Base):
    __tablename__ = "test_run_signal_snapshot_entries"
    __table_args__ = (
        Index("ix_snapshot_entries_snapshot", "snapshot_id"),
        Index("ix_snapshot_entries_signal", "live_signal_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    snapshot_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("test_run_signal_snapshots.test_run_id", ondelete="CASCADE"),
        nullable=False,
    )
    live_signal_id: Mapped[int | None] = mapped_column(
        BIGINT_PK,
        ForeignKey("signals.id", ondelete="SET NULL"),
        nullable=True,
    )
    signal_key: Mapped[str] = mapped_column(String(128), nullable=False)
    name: Mapped[str] = mapped_column(String(255), nullable=False)
    io_direction: Mapped[SignalIODirection] = mapped_column(
        SAEnum(SignalIODirection, name="signal_io_direction_enum"),
        nullable=False,
    )
    allocation_channel_id: Mapped[int | None] = mapped_column(
        BIGINT_PK,
        ForeignKey("channels.id", ondelete="SET NULL"),
        nullable=True,
    )
    allocation_metadata: Mapped[dict[str, Any] | None] = mapped_column(JSONB, nullable=True)
    entry_metadata: Mapped[dict[str, Any]] = mapped_column(
        "metadata",
        JSONB,
        nullable=False,
        server_default=text("'{}'::jsonb"),
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    snapshot: Mapped["TestRunSignalSnapshot"] = relationship(
        "TestRunSignalSnapshot",
        back_populates="entries",
        lazy="selectin",
    )
    live_signal: Mapped["Signal | None"] = relationship(
        "Signal",
        back_populates="snapshot_entries",
        lazy="selectin",
    )
    allocation_channel: Mapped["Channel | None"] = relationship(
        "Channel",
        lazy="selectin",
    )

