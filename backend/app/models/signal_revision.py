from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from sqlalchemy import DateTime, ForeignKey, Index, Integer, JSON, String, UniqueConstraint, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:
    from app.models.workspace import Workspace


class SignalListRevision(Base):
    __tablename__: str = "signal_list_revisions"
    __table_args__: tuple[object, ...] = (
        UniqueConstraint("workspace_id", "revision_no", name="uq_signal_list_revisions_workspace_no"),
        Index("ix_signal_list_revisions_workspace_status", "workspace_id", "status", "created_at"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(BIGINT_PK, ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False)
    revision_no: Mapped[int] = mapped_column(Integer, nullable=False)
    status: Mapped[str] = mapped_column(String(16), nullable=False, server_default="active")
    source_hash: Mapped[str | None] = mapped_column(String(64), nullable=True)
    schema_version: Mapped[int] = mapped_column(Integer, nullable=False, server_default="1")
    rows_count: Mapped[int] = mapped_column(Integer, nullable=False, server_default="0")
    content_hash: Mapped[str] = mapped_column(String(64), nullable=False)
    created_by: Mapped[str | None] = mapped_column(String(128), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now(), nullable=False)
    activated_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")
    items: Mapped[list["SignalListRevisionItem"]] = relationship(
        "SignalListRevisionItem", back_populates="revision", cascade="all, delete-orphan", lazy="selectin"
    )


class SignalListRevisionItem(Base):
    __tablename__: str = "signal_list_revision_items"
    __table_args__: tuple[object, ...] = (
        UniqueConstraint("revision_id", "order_index", name="uq_signal_list_revision_items_order"),
        Index("ix_signal_list_revision_items_revision", "revision_id", "order_index"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    revision_id: Mapped[int] = mapped_column(BIGINT_PK, ForeignKey("signal_list_revisions.id", ondelete="CASCADE"), nullable=False)
    live_signal_id: Mapped[int | None] = mapped_column(BIGINT_PK, ForeignKey("signals.id", ondelete="SET NULL"), nullable=True)
    order_index: Mapped[int] = mapped_column(Integer, nullable=False)
    signal_key: Mapped[str] = mapped_column(String(128), nullable=False)
    snapshot: Mapped[dict[str, object]] = mapped_column(JSON, nullable=False, server_default="{}")

    revision: Mapped[SignalListRevision] = relationship("SignalListRevision", back_populates="items")


class SignalTestRunPlan(Base):
    __tablename__: str = "signal_test_run_plans"

    job_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    workspace_id: Mapped[int] = mapped_column(BIGINT_PK, ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False)
    revision_id: Mapped[int] = mapped_column(BIGINT_PK, ForeignKey("signal_list_revisions.id", ondelete="RESTRICT"), nullable=False)
    content_hash: Mapped[str] = mapped_column(String(64), nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now(), nullable=False)

    items: Mapped[list["SignalTestRunPlanItem"]] = relationship(
        "SignalTestRunPlanItem", back_populates="plan", cascade="all, delete-orphan", lazy="selectin"
    )


class SignalTestRunPlanItem(Base):
    __tablename__: str = "signal_test_run_plan_items"
    __table_args__: tuple[object, ...] = (UniqueConstraint("job_id", "order_index", name="uq_signal_test_run_plan_items_order"),)

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    job_id: Mapped[str] = mapped_column(String(64), ForeignKey("signal_test_run_plans.job_id", ondelete="CASCADE"), nullable=False)
    revision_item_id: Mapped[int] = mapped_column(BIGINT_PK, ForeignKey("signal_list_revision_items.id", ondelete="RESTRICT"), nullable=False)
    order_index: Mapped[int] = mapped_column(Integer, nullable=False)
    snapshot: Mapped[dict[str, object]] = mapped_column(JSON, nullable=False, server_default="{}")

    plan: Mapped[SignalTestRunPlan] = relationship("SignalTestRunPlan", back_populates="items")
