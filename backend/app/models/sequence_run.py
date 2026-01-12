"""Sequence run persistence models."""
from __future__ import annotations

from datetime import datetime
from enum import Enum
from typing import Optional

from sqlalchemy import (
    CheckConstraint,
    DateTime,
    Enum as SAEnum,
    ForeignKey,
    Index,
    Integer,
    Text,
    func,
    text,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK


class SequenceRunStatus(str, Enum):
    PENDING = "pending"
    RUNNING = "running"
    CANCELLING = "cancelling"
    COMPLETED = "completed"
    STOPPED = "stopped"
    ERROR = "error"


class SequenceRunStepStatus(str, Enum):
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    ERROR = "error"
    CANCELLED = "cancelled"


class SequenceRun(Base):
    """Sequence execution record created only through TestRun-driven execution."""
    __tablename__ = "sequence_runs"

    __table_args__ = (
        CheckConstraint(
            "status IN ('pending','running','cancelling','completed','stopped','error')",
            name="ck_sequence_runs_status",
        ),
        Index("ix_sequence_runs_sequence_started", "sequence_id", "started_at"),
        Index(
            "uq_sequence_runs_active",
            "sequence_id",
            unique=True,
            postgresql_where=text("status IN ('pending','running','cancelling')"),
        ),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    sequence_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("sequences.id", ondelete="CASCADE"), nullable=False
    )
    status: Mapped[SequenceRunStatus] = mapped_column(
        SAEnum(
            SequenceRunStatus,
            name="sequence_run_status_enum",
            values_callable=lambda enum: [member.value for member in enum],
        ),
        nullable=False,
        server_default=SequenceRunStatus.PENDING.value,
    )
    started_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    finished_at: Mapped[Optional[datetime]] = mapped_column(
        DateTime(timezone=True), nullable=True
    )
    error_message: Mapped[Optional[str]] = mapped_column(Text, nullable=True)
    current_step_index: Mapped[int] = mapped_column(Integer, nullable=False, server_default="0")

    sequence = relationship("Sequence", back_populates="runs")
    steps = relationship(
        "SequenceRunStep",
        back_populates="run",
        cascade="all, delete-orphan",
        order_by="SequenceRunStep.order_index",
        lazy="selectin",
    )


class SequenceRunStep(Base):
    __tablename__ = "sequence_run_steps"

    __table_args__ = (
        CheckConstraint(
            "status IN ('pending','running','completed','error','cancelled')",
            name="ck_sequence_run_steps_status",
        ),
        Index("ix_sequence_run_steps_run_order", "run_id", "order_index"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    run_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("sequence_runs.id", ondelete="CASCADE"), nullable=False
    )
    sequence_step_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("sequence_steps.id", ondelete="CASCADE"), nullable=False
    )
    order_index: Mapped[int] = mapped_column(Integer, nullable=False)
    status: Mapped[SequenceRunStepStatus] = mapped_column(
        SAEnum(
            SequenceRunStepStatus,
            name="sequence_run_step_status_enum",
            values_callable=lambda enum: [member.value for member in enum],
        ),
        nullable=False,
        server_default=SequenceRunStepStatus.PENDING.value,
    )
    started_at: Mapped[Optional[datetime]] = mapped_column(
        DateTime(timezone=True), nullable=True
    )
    finished_at: Mapped[Optional[datetime]] = mapped_column(
        DateTime(timezone=True), nullable=True
    )
    error_message: Mapped[Optional[str]] = mapped_column(Text, nullable=True)
    elapsed_ms: Mapped[Optional[int]] = mapped_column(Integer, nullable=True)

    run = relationship("SequenceRun", back_populates="steps")
