"""add signal verification evidence

Revision ID: a1b2c3d4e5f6
Revises: f4b5c6d7e8f9
Create Date: 2026-06-23 00:00:00.000000
"""

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import JSONB


revision = "a1b2c3d4e5f6"
down_revision = "f4b5c6d7e8f9"
branch_labels = None
depends_on = None


BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    op.create_table(
        "signal_verification_evidence",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("test_run_id", sa.String(length=64), nullable=False),
        sa.Column("evidence_id", sa.String(length=64), nullable=False),
        sa.Column("signal_id", BIGINT_PK, nullable=False),
        sa.Column("signal_path", sa.String(length=255), nullable=False),
        sa.Column("expected_path", sa.String(length=255), nullable=False),
        sa.Column("actual_report_path", sa.String(length=255), nullable=True),
        sa.Column("source_ied", sa.String(length=128), nullable=True),
        sa.Column("endpoint_id", sa.String(length=255), nullable=True),
        sa.Column("rpt_id", sa.String(length=255), nullable=True),
        sa.Column("dataset", sa.String(length=255), nullable=True),
        sa.Column("observed_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("latency_ms", sa.Integer(), nullable=True),
        sa.Column("quality", sa.String(length=32), nullable=True),
        sa.Column("freshness", sa.String(length=16), nullable=True),
        sa.Column("evidence_status", sa.String(length=32), nullable=False),
        sa.Column("reason_code", sa.String(length=64), nullable=False),
        sa.Column("source_generation", BIGINT_PK, nullable=True),
        sa.Column("source_report_sequence_generation", BIGINT_PK, nullable=True),
        sa.Column("source_report_sequence_number", BIGINT_PK, nullable=True),
        sa.Column("source_report_sub_sequence_number", BIGINT_PK, nullable=True),
        sa.Column("report_reason", sa.String(length=64), nullable=True),
        sa.Column("signal_value", JSONB, nullable=True),
        sa.Column("timestamp_summary", JSONB, nullable=True),
        sa.Column("stale_reason", sa.String(length=128), nullable=True),
        sa.Column("evidence_kind", sa.String(length=32), nullable=True),
        sa.Column("diagnostics", JSONB, nullable=False, server_default=sa.text("'[]'::jsonb")),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
        sa.UniqueConstraint("workspace_id", "evidence_id", name="uq_signal_verification_evidence_workspace_evidence"),
    )
    op.create_index(
        "ix_signal_verification_evidence_workspace_run_created",
        "signal_verification_evidence",
        ["workspace_id", "test_run_id", "created_at", "id"],
        unique=False,
    )
    op.create_index(
        "ix_signal_verification_evidence_workspace_signal_created",
        "signal_verification_evidence",
        ["workspace_id", "signal_id", "created_at"],
        unique=False,
    )
    op.create_index(
        "ix_signal_verification_evidence_workspace_status_created",
        "signal_verification_evidence",
        ["workspace_id", "evidence_status", "created_at"],
        unique=False,
    )

    op.create_table(
        "signal_verification_evidence_sets",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("test_run_id", sa.String(length=64), nullable=False),
        sa.Column("summary", JSONB, nullable=False, server_default=sa.text("'{}'::jsonb")),
        sa.Column("diagnostics", JSONB, nullable=False, server_default=sa.text("'[]'::jsonb")),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
        sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
        sa.UniqueConstraint("workspace_id", "test_run_id", name="uq_signal_verification_evidence_sets_run"),
    )
    op.create_index(
        "ix_signal_verification_evidence_sets_workspace_created",
        "signal_verification_evidence_sets",
        ["workspace_id", "created_at", "id"],
        unique=False,
    )
    op.create_index(
        "ix_signal_verification_evidence_sets_workspace_run",
        "signal_verification_evidence_sets",
        ["workspace_id", "test_run_id"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index("ix_signal_verification_evidence_sets_workspace_run", table_name="signal_verification_evidence_sets")
    op.drop_index("ix_signal_verification_evidence_sets_workspace_created", table_name="signal_verification_evidence_sets")
    op.drop_table("signal_verification_evidence_sets")

    op.drop_index("ix_signal_verification_evidence_workspace_status_created", table_name="signal_verification_evidence")
    op.drop_index("ix_signal_verification_evidence_workspace_signal_created", table_name="signal_verification_evidence")
    op.drop_index("ix_signal_verification_evidence_workspace_run_created", table_name="signal_verification_evidence")
    op.drop_table("signal_verification_evidence")
