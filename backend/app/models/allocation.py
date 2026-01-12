"""Allocation models that belong to a TestRun."""
from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING, Any

from sqlalchemy import DateTime, ForeignKey, Index, Text, UniqueConstraint
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing helpers only
    from app.models.channel import Channel
    from app.models.test_run import TestRun


class Allocation(Base):
    """Channel-to-signal mapping captured for a specific test run.

    Channel-mode allocation: every entry binds a `Channel` and leaves all signal fields null.
    Signal-mode allocation: entries still bind physical channels but may populate signal metadata to
    trace back to the optional `SignalSnapshot` referenced by the owning `TestRun`.
    """

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

    test_run: Mapped["TestRun"] = relationship(
        "TestRun",
        back_populates="allocation",
        lazy="selectin",
    )
    entries: Mapped[list["AllocationEntry"]] = relationship(
        "AllocationEntry",
        back_populates="allocation",
        cascade="all, delete-orphan",
        lazy="selectin",
    )


class AllocationEntry(Base):
    """Single channel binding inside an Allocation."""

    __tablename__ = "test_run_allocation_entries"
    __table_args__ = (
        UniqueConstraint("allocation_id", "channel_id", name="uq_allocation_entry_channel"),
        Index("ix_allocation_entries_channel", "channel_id"),
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
    signal_key: Mapped[str | None] = mapped_column(nullable=True)
    signal_metadata: Mapped[dict[str, Any] | None] = mapped_column(JSONB, nullable=True)

    allocation: Mapped["Allocation"] = relationship("Allocation", back_populates="entries")
    channel: Mapped["Channel"] = relationship("Channel", lazy="selectin")
