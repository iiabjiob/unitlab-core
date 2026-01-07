"""Persistence models for simple channel-based test runs."""
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
    Text,
    UniqueConstraint,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing helpers only
    from app.models.project import Project
    from app.models.channel import Channel


def _enum_values(enum_cls: type[Enum]) -> list[str]:
    return [member.value for member in enum_cls]


class TestRunStatus(str, Enum):
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"


class TestRun(Base):
    __tablename__ = "test_runs"
    __table_args__ = (
        Index("ix_test_runs_project_created_at", "project_id", "created_at"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    project_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("projects.id", ondelete="CASCADE"), nullable=False
    )
    name: Mapped[str] = mapped_column(String(120), nullable=False)
    status: Mapped[TestRunStatus] = mapped_column(
        SAEnum(TestRunStatus, name="test_run_status_enum", values_callable=_enum_values),
        nullable=False,
        default=TestRunStatus.PENDING,
        server_default=TestRunStatus.PENDING.value,
    )
    settings: Mapped[dict] = mapped_column(
        JSON, nullable=False, default=lambda: {"delay_ms": 500}
    )
    current_step_index: Mapped[int] = mapped_column(Integer, nullable=False, default=0, server_default="0")
    error_message: Mapped[str | None] = mapped_column(Text, nullable=True)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )
    started_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    finished_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    project: Mapped["Project"] = relationship(
        "Project", back_populates="test_runs", lazy="selectin"
    )
    steps: Mapped[list["TestRunStep"]] = relationship(
        "TestRunStep",
        back_populates="test_run",
        cascade="all, delete-orphan",
        order_by="TestRunStep.order_index",
        lazy="selectin",
    )


class TestRunStep(Base):
    __tablename__ = "test_run_steps"
    __table_args__ = (
        UniqueConstraint("test_run_id", "order_index", name="uq_test_run_steps_order"),
        Index("ix_test_run_steps_run_order", "test_run_id", "order_index"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    test_run_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("test_runs.id", ondelete="CASCADE"), nullable=False
    )
    order_index: Mapped[int] = mapped_column(Integer, nullable=False)
    channel_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("channels.id", ondelete="SET NULL"), nullable=True
    )

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    test_run: Mapped["TestRun"] = relationship("TestRun", back_populates="steps", lazy="selectin")
    channel: Mapped["Channel"] = relationship("Channel", back_populates="test_steps", lazy="selectin")
