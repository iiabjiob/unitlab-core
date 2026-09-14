from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING, Any

from sqlalchemy import DateTime, ForeignKey, Index, Integer, JSON, String, UniqueConstraint, func, text
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing only
    from app.models.workspace import Workspace


class SignalVerificationEvidence(Base):
    __tablename__ = "signal_verification_evidence"

    __table_args__ = (
        UniqueConstraint("workspace_id", "evidence_id", name="uq_signal_verification_evidence_workspace_evidence"),
        Index("ix_signal_verification_evidence_workspace_run_created", "workspace_id", "test_run_id", "created_at", "id"),
        Index("ix_signal_verification_evidence_workspace_signal_created", "workspace_id", "signal_id", "created_at"),
        Index("ix_signal_verification_evidence_workspace_status_created", "workspace_id", "evidence_status", "created_at"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    test_run_id: Mapped[str] = mapped_column(String(64), nullable=False)
    signal_list_revision_id: Mapped[int | None] = mapped_column(
        BIGINT_PK,
        ForeignKey("signal_list_revisions.id", ondelete="RESTRICT"),
        nullable=True,
    )
    evidence_id: Mapped[str] = mapped_column(String(64), nullable=False)
    signal_id: Mapped[int] = mapped_column(BIGINT_PK, nullable=False)
    signal_path: Mapped[str] = mapped_column(String(255), nullable=False)
    expected_path: Mapped[str] = mapped_column(String(255), nullable=False)
    actual_report_path: Mapped[str | None] = mapped_column(String(255), nullable=True)
    source_ied: Mapped[str | None] = mapped_column(String(128), nullable=True)
    endpoint_id: Mapped[str | None] = mapped_column(String(255), nullable=True)
    rpt_id: Mapped[str | None] = mapped_column(String(255), nullable=True)
    dataset: Mapped[str | None] = mapped_column(String(255), nullable=True)
    observed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    latency_ms: Mapped[int | None] = mapped_column(Integer, nullable=True)
    quality: Mapped[str | None] = mapped_column(String(32), nullable=True)
    freshness: Mapped[str | None] = mapped_column(String(16), nullable=True)
    evidence_status: Mapped[str] = mapped_column(String(32), nullable=False)
    reason_code: Mapped[str] = mapped_column(String(64), nullable=False)
    source_generation: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    source_report_sequence_generation: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    source_report_sequence_number: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    source_report_sub_sequence_number: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    report_reason: Mapped[str | None] = mapped_column(String(64), nullable=True)
    signal_value: Mapped[Any | None] = mapped_column(JSONB().with_variant(JSON(), "sqlite"), nullable=True)
    timestamp_summary: Mapped[dict[str, Any] | None] = mapped_column(
        JSONB().with_variant(JSON(), "sqlite"),
        nullable=True,
    )
    stale_reason: Mapped[str | None] = mapped_column(String(128), nullable=True)
    evidence_kind: Mapped[str | None] = mapped_column(String(32), nullable=True)
    diagnostics: Mapped[list] = mapped_column(
        JSONB().with_variant(JSON(), "sqlite"),
        nullable=False,
        server_default=text("'[]'::jsonb"),
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")


class SignalVerificationEvidenceSet(Base):
    __tablename__ = "signal_verification_evidence_sets"

    __table_args__ = (
        UniqueConstraint("workspace_id", "test_run_id", name="uq_signal_verification_evidence_sets_run"),
        Index("ix_signal_verification_evidence_sets_workspace_created", "workspace_id", "created_at", "id"),
        Index("ix_signal_verification_evidence_sets_workspace_run", "workspace_id", "test_run_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    workspace_id: Mapped[int] = mapped_column(
        BIGINT_PK,
        ForeignKey("workspaces.id", ondelete="CASCADE"),
        nullable=False,
    )
    test_run_id: Mapped[str] = mapped_column(String(64), nullable=False)
    summary: Mapped[dict] = mapped_column(
        JSONB().with_variant(JSON(), "sqlite"),
        nullable=False,
        server_default=text("'{}'::jsonb"),
    )
    diagnostics: Mapped[list] = mapped_column(
        JSONB().with_variant(JSON(), "sqlite"),
        nullable=False,
        server_default=text("'[]'::jsonb"),
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    workspace: Mapped["Workspace"] = relationship("Workspace", lazy="selectin")
