from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from sqlalchemy import (
    DateTime,
    ForeignKey,
    Index,
    JSON,
    Integer,
    String,
    UniqueConstraint,
    func,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing only
    from app.models.channel import Channel
    from app.models.signal import Signal
    from app.models.workspace import Workspace


class SignalSheet(Base):
    __tablename__ = "signal_sheets"

    __table_args__ = (
        UniqueConstraint("workspace_id", name="uq_signal_sheets_workspace"),
        Index("ix_signal_sheets_workspace", "workspace_id"),
        Index("ix_signal_sheets_workspace_updated", "workspace_id", "updated_at"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    source_filename: Mapped[str | None] = mapped_column(String(255), nullable=True)
    source_hash: Mapped[str | None] = mapped_column(String(64), nullable=True)
    rows_count: Mapped[int] = mapped_column(Integer, nullable=False, server_default="0")
    schema_version: Mapped[int] = mapped_column(Integer, nullable=False, server_default="2")
    data: Mapped[dict] = mapped_column(JSON, nullable=False, server_default="{}")
    import_meta: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")


class SignalSheetPreset(Base):
    __tablename__ = "signal_sheet_presets"

    __table_args__ = (
        UniqueConstraint("workspace_id", "name", name="uq_signal_sheet_presets_workspace_name"),
        Index("ix_signal_sheet_presets_workspace", "workspace_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    name: Mapped[str] = mapped_column(String(120), nullable=False)
    import_meta: Mapped[dict] = mapped_column(JSON, nullable=False, server_default="{}")
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")


class SignalAllocation(Base):
    __tablename__ = "signal_allocations"

    __table_args__ = (
        UniqueConstraint("workspace_id", "signal_id", name="uq_signal_allocations_signal"),
        UniqueConstraint("workspace_id", "channel_id", name="uq_signal_allocations_channel"),
        Index("ix_signal_allocations_workspace", "workspace_id"),
        Index("ix_signal_allocations_channel", "channel_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    signal_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("signals.id", ondelete="CASCADE"),
        nullable=False,
    )
    channel_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("channels.id", ondelete="CASCADE"),
        nullable=False,
    )
    allocation_meta: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")
    signal: Mapped["Signal"] = relationship("Signal", back_populates="allocation", lazy="selectin")
    channel: Mapped["Channel"] = relationship("Channel", lazy="selectin")


class SignalAllocationEvent(Base):
    __tablename__ = "signal_allocation_events"

    __table_args__ = (
        Index("ix_signal_allocation_events_workspace_created", "workspace_id", "created_at", "id"),
        Index("ix_signal_allocation_events_workspace_operation", "workspace_id", "operation", "created_at"),
        Index("ix_signal_allocation_events_workspace_signal", "workspace_id", "signal_id", "created_at"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    operation: Mapped[str] = mapped_column(String(32), nullable=False)
    source: Mapped[str] = mapped_column(String(32), nullable=False, server_default="api")
    signal_id: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    previous_channel_id: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    channel_id: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    requested_count: Mapped[int] = mapped_column(Integer, nullable=False, server_default="0")
    changed_count: Mapped[int] = mapped_column(Integer, nullable=False, server_default="0")
    skipped_count: Mapped[int] = mapped_column(Integer, nullable=False, server_default="0")
    rejected_count: Mapped[int] = mapped_column(Integer, nullable=False, server_default="0")
    payload: Mapped[dict] = mapped_column(JSON, nullable=False, server_default="{}")
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")


class SignalTestRunStepEvidence(Base):
    __tablename__ = "signal_test_run_step_evidence"

    __table_args__ = (
        Index("ix_signal_test_run_evidence_workspace_job_order", "workspace_id", "job_id", "order_index"),
        Index("ix_signal_test_run_evidence_workspace_created", "workspace_id", "created_at", "id"),
        Index("ix_signal_test_run_evidence_workspace_signal", "workspace_id", "signal_id", "created_at"),
        Index("ix_signal_test_run_evidence_workspace_status", "workspace_id", "status", "created_at"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    job_id: Mapped[str] = mapped_column(String(64), nullable=False)
    attempt_id: Mapped[str | None] = mapped_column(String(64), nullable=True)
    attempt_no: Mapped[int] = mapped_column(Integer, nullable=False, server_default="0")
    order_index: Mapped[int] = mapped_column(Integer, nullable=False)
    signal_id: Mapped[int] = mapped_column(BIGINT_PK, nullable=False)
    allocation_id: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    channel_id: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    device_id: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    unit_id: Mapped[str | None] = mapped_column(String(128), nullable=True)
    channel_index: Mapped[int | None] = mapped_column(Integer, nullable=True)
    channel_type: Mapped[str | None] = mapped_column(String(32), nullable=True)
    status: Mapped[str] = mapped_column(String(32), nullable=False)
    reason: Mapped[str | None] = mapped_column(String(128), nullable=True)
    result_state: Mapped[str | None] = mapped_column(String(64), nullable=True)
    command_payload: Mapped[dict] = mapped_column(JSON, nullable=False, server_default="{}")
    tested_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")
