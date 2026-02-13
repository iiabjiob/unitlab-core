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
    String,
    UniqueConstraint,
    func,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing only
    from app.models.workspace import Workspace


class SignalSnapshotStatus(str, Enum):
    DRAFT = "draft"
    LOCKED = "locked"


class SignalSnapshot(Base):
    __tablename__ = "signal_snapshots"

    __table_args__ = (
        Index("ix_signal_snapshots_workspace", "workspace_id"),
        Index("ix_signal_snapshots_workspace_updated", "workspace_id", "updated_at"),
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
            values_callable=lambda enum: [member.value for member in enum],
            native_enum=False,
        ),
        nullable=False,
        server_default=SignalSnapshotStatus.DRAFT.value,
    )
    source_filename: Mapped[str | None] = mapped_column(String(255), nullable=True)
    source_hash: Mapped[str | None] = mapped_column(String(64), nullable=True)
    rows_count: Mapped[int] = mapped_column(Integer, nullable=False, server_default="0")
    schema_version: Mapped[int] = mapped_column(Integer, nullable=False, server_default="2")
    data: Mapped[dict] = mapped_column(JSON, nullable=False)
    import_meta: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    locked_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")
    allocation: Mapped["SignalSnapshotAllocation | None"] = relationship(
        "SignalSnapshotAllocation",
        back_populates="snapshot",
        cascade="all, delete-orphan",
        uselist=False,
        lazy="selectin",
    )


class SignalSnapshotAllocation(Base):
    __tablename__ = "signal_snapshot_allocations"

    __table_args__ = (
        UniqueConstraint("signal_snapshot_id", name="uq_signal_snapshot_allocations_snapshot"),
        Index("ix_signal_snapshot_allocations_workspace", "workspace_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    signal_snapshot_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("signal_snapshots.id", ondelete="CASCADE"),
        nullable=False,
    )
    mapping: Mapped[list[dict]] = mapped_column(JSON, nullable=False, server_default="[]")
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    snapshot: Mapped[SignalSnapshot] = relationship(
        "SignalSnapshot",
        back_populates="allocation",
        lazy="selectin",
    )
