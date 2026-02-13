from __future__ import annotations

from datetime import datetime
from enum import Enum
from typing import TYPE_CHECKING

from sqlalchemy import (
    DateTime,
    Enum as SAEnum,
    ForeignKey,
    Index,
    Integer,
    JSON,
    Text,
    String,
    UniqueConstraint,
    func,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.infrastructure.db.database import Base
from app.models.signal import SignalIODirection
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing only
    from app.models.channel import Channel
    from app.models.sequence import Sequence
    from app.models.signal import Signal
    from app.models.workspace import Workspace


class TestRunStatus(str, Enum):
    CREATED = "created"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"


class TestRun(Base):
    __tablename__ = "test_runs"

    __table_args__ = (
        Index("ix_test_runs_workspace_created", "workspace_id", "created_at"),
        Index("ix_test_runs_workspace_status", "workspace_id", "status"),
        Index("ix_test_runs_source", "source_test_run_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    source_test_run_id: Mapped[int | None] = mapped_column(
        BIGINT_PK,
        ForeignKey("test_runs.id", ondelete="SET NULL"),
        nullable=True,
    )
    allocation_revision: Mapped[int] = mapped_column(Integer, nullable=False, server_default="1")
    status: Mapped[TestRunStatus] = mapped_column(
        SAEnum(
            TestRunStatus,
            name="test_run_status_enum",
            values_callable=lambda enum: [member.value for member in enum],
            native_enum=False,
        ),
        nullable=False,
        server_default=TestRunStatus.CREATED.value,
    )
    execution_meta: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    started_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    finished_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")
    sequence_links: Mapped[list["TestRunSequence"]] = relationship(
        "TestRunSequence",
        back_populates="test_run",
        cascade="all, delete-orphan",
        order_by="TestRunSequence.order_index",
        lazy="selectin",
    )
    allocation: Mapped["TestRunAllocation | None"] = relationship(
        "TestRunAllocation",
        back_populates="test_run",
        cascade="all, delete-orphan",
        uselist=False,
        lazy="selectin",
    )
    snapshot: Mapped["TestRunSignalSnapshot | None"] = relationship(
        "TestRunSignalSnapshot",
        back_populates="test_run",
        cascade="all, delete-orphan",
        uselist=False,
        lazy="selectin",
    )
    source_test_run: Mapped["TestRun | None"] = relationship(
        "TestRun",
        remote_side=[id],
        lazy="selectin",
    )


class TestRunSequence(Base):
    __tablename__ = "test_run_sequences"

    __table_args__ = (
        UniqueConstraint("test_run_id", "order_index", name="uq_test_run_sequences_order"),
        UniqueConstraint("test_run_id", "sequence_id", name="uq_test_run_sequences_pair"),
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
    order_index: Mapped[int] = mapped_column(Integer, nullable=False)

    test_run: Mapped[TestRun] = relationship("TestRun", back_populates="sequence_links", lazy="selectin")
    sequence: Mapped["Sequence"] = relationship("Sequence", lazy="selectin")


class TestRunAllocation(Base):
    __tablename__ = "test_run_allocations"

    __table_args__ = (
        UniqueConstraint("test_run_id", name="uq_test_run_allocations_test_run"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    test_run_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("test_runs.id", ondelete="CASCADE"),
        nullable=False,
    )
    notes: Mapped[str | None] = mapped_column(Text, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    test_run: Mapped[TestRun] = relationship("TestRun", back_populates="allocation", lazy="selectin")
    entries: Mapped[list["TestRunAllocationEntry"]] = relationship(
        "TestRunAllocationEntry",
        back_populates="allocation",
        cascade="all, delete-orphan",
        lazy="selectin",
    )


class TestRunAllocationEntry(Base):
    __tablename__ = "test_run_allocation_entries"

    __table_args__ = (
        UniqueConstraint("allocation_id", "channel_id", name="uq_allocation_entry_channel"),
        Index("ix_test_run_allocation_entries_channel", "channel_id"),
        Index("ix_test_run_allocation_entries_signal", "signal_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    allocation_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("test_run_allocations.id", ondelete="CASCADE"),
        nullable=False,
    )
    channel_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("channels.id", ondelete="RESTRICT"),
        nullable=False,
    )
    signal_id: Mapped[int | None] = mapped_column(
        BIGINT_PK,
        ForeignKey("signals.id", ondelete="SET NULL"),
        nullable=True,
    )
    signal_metadata: Mapped[dict | None] = mapped_column(JSON, nullable=True)

    allocation: Mapped[TestRunAllocation] = relationship(
        "TestRunAllocation",
        back_populates="entries",
        lazy="selectin",
    )
    channel: Mapped["Channel"] = relationship("Channel", lazy="selectin")
    signal: Mapped["Signal | None"] = relationship("Signal", lazy="selectin")


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

    test_run: Mapped[TestRun] = relationship("TestRun", back_populates="snapshot", lazy="selectin")
    entries: Mapped[list["TestRunSignalSnapshotEntry"]] = relationship(
        "TestRunSignalSnapshotEntry",
        back_populates="snapshot",
        cascade="all, delete-orphan",
        lazy="selectin",
    )


class TestRunSignalSnapshotEntry(Base):
    __tablename__ = "test_run_signal_snapshot_entries"

    __table_args__ = (
        Index("ix_test_run_signal_snapshot_entries_snapshot", "snapshot_id"),
        Index("ix_test_run_signal_snapshot_entries_signal", "live_signal_id"),
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
        SAEnum(
            SignalIODirection,
            name="signal_io_direction_enum",
            values_callable=lambda enum: [member.value for member in enum],
            native_enum=False,
        ),
        nullable=False,
    )
    allocation_channel_id: Mapped[int | None] = mapped_column(
        BIGINT_PK,
        ForeignKey("channels.id", ondelete="SET NULL"),
        nullable=True,
    )
    allocation_metadata: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    entry_metadata: Mapped[dict] = mapped_column("metadata", JSON, nullable=False, server_default="{}")
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    snapshot: Mapped[TestRunSignalSnapshot] = relationship(
        "TestRunSignalSnapshot",
        back_populates="entries",
        lazy="selectin",
    )
    signal: Mapped["Signal | None"] = relationship("Signal", lazy="selectin")
