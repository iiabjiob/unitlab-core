"""add signal test run step evidence

Revision ID: f2a3b4c5d6e8
Revises: f1a2b3c4d5e7
Create Date: 2026-05-24 19:20:00.000000
"""

from alembic import op
import sqlalchemy as sa


revision = "f2a3b4c5d6e8"
down_revision = "f1a2b3c4d5e7"
branch_labels = None
depends_on = None


BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    op.create_table(
        "signal_test_run_step_evidence",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("job_id", sa.String(length=64), nullable=False),
        sa.Column("attempt_id", sa.String(length=64), nullable=True),
        sa.Column("attempt_no", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("order_index", sa.Integer(), nullable=False),
        sa.Column("signal_id", BIGINT_PK, nullable=False),
        sa.Column("allocation_id", BIGINT_PK, nullable=True),
        sa.Column("channel_id", BIGINT_PK, nullable=True),
        sa.Column("device_id", BIGINT_PK, nullable=True),
        sa.Column("unit_id", sa.String(length=128), nullable=True),
        sa.Column("channel_index", sa.Integer(), nullable=True),
        sa.Column("channel_type", sa.String(length=32), nullable=True),
        sa.Column("status", sa.String(length=32), nullable=False),
        sa.Column("reason", sa.String(length=128), nullable=True),
        sa.Column("result_state", sa.String(length=64), nullable=True),
        sa.Column("command_payload", sa.JSON(), nullable=False, server_default=sa.text("'{}'")),
        sa.Column("tested_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
    )
    op.create_index(
        "ix_signal_test_run_evidence_workspace_job_order",
        "signal_test_run_step_evidence",
        ["workspace_id", "job_id", "order_index"],
        unique=False,
    )
    op.create_index(
        "ix_signal_test_run_evidence_workspace_created",
        "signal_test_run_step_evidence",
        ["workspace_id", "created_at", "id"],
        unique=False,
    )
    op.create_index(
        "ix_signal_test_run_evidence_workspace_signal",
        "signal_test_run_step_evidence",
        ["workspace_id", "signal_id", "created_at"],
        unique=False,
    )
    op.create_index(
        "ix_signal_test_run_evidence_workspace_status",
        "signal_test_run_step_evidence",
        ["workspace_id", "status", "created_at"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index("ix_signal_test_run_evidence_workspace_status", table_name="signal_test_run_step_evidence")
    op.drop_index("ix_signal_test_run_evidence_workspace_signal", table_name="signal_test_run_step_evidence")
    op.drop_index("ix_signal_test_run_evidence_workspace_created", table_name="signal_test_run_step_evidence")
    op.drop_index("ix_signal_test_run_evidence_workspace_job_order", table_name="signal_test_run_step_evidence")
    op.drop_table("signal_test_run_step_evidence")
