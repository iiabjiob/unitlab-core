"""Signal snapshot allocation persistence model."""
from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING, Any

from sqlalchemy import DateTime, ForeignKey, Index, UniqueConstraint, text
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing helpers only
    from app.models.signal_snapshot import SignalSnapshot
    from app.models.workspace import Workspace


class SignalSnapshotAllocation(Base):
    """Stores per-snapshot channel mapping metadata."""

    __tablename__ = "signal_snapshot_allocations"
    __table_args__ = (
        UniqueConstraint(
            "signal_snapshot_id",
            name="uq_snapshot_allocation_signal_snapshot",
        ),
        Index("ix_snapshot_allocations_workspace", "workspace_id"),
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
    mapping: Mapped[list[dict[str, Any]]] = mapped_column(
        JSONB,
        nullable=False,
        server_default=text("'[]'::jsonb"),
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
        nullable=False,
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
        onupdate=func.now(),
        nullable=False,
    )

    snapshot: Mapped["SignalSnapshot"] = relationship(
        "SignalSnapshot",
        back_populates="allocation",
        lazy="selectin",
    )
    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")
