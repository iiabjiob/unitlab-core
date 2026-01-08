from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from sqlalchemy import DateTime, ForeignKey, Index, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing helpers only
    from app.models.channel import Channel
    from app.models.datapoint import Datapoint
    from app.models.project import Project


class Allocation(Base):
    __tablename__ = "allocations"
    __table_args__ = (
        UniqueConstraint("datapoint_id", name="uq_allocation_datapoint"),
        UniqueConstraint("channel_id", name="uq_allocation_channel"),
        Index("ix_allocations_project_datapoint", "project_id", "datapoint_id"),
        Index("ix_allocations_project_channel", "project_id", "channel_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    project_id: Mapped[int] = mapped_column(
        ForeignKey("projects.id", ondelete="CASCADE"), nullable=False, index=True
    )
    datapoint_id: Mapped[int] = mapped_column(
        ForeignKey("datapoints.id", ondelete="CASCADE"), nullable=False, index=True
    )
    channel_id: Mapped[int | None] = mapped_column(
        ForeignKey("channels.id", ondelete="SET NULL"), nullable=True, index=True
    )

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    project: Mapped["Project"] = relationship(
        "Project", back_populates="allocations", lazy="selectin"
    )
    datapoint: Mapped["Datapoint"] = relationship(
        "Datapoint", back_populates="allocations", lazy="selectin"
    )
    channel: Mapped["Channel | None"] = relationship(
        "Channel", back_populates="allocations", lazy="selectin"
    )
