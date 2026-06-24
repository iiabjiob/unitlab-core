"""add signal verification runs

Revision ID: b4c5d6e7f8a9
Revises: a1b2c3d4e5f6
Create Date: 2026-06-24 00:00:00.000000
"""

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import JSONB


revision = "b4c5d6e7f8a9"
down_revision = "a1b2c3d4e5f6"
branch_labels = None
depends_on = None


BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    op.create_table(
        "signal_verification_runs",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("test_run_id", sa.String(length=64), nullable=False),
        sa.Column("payload", JSONB, nullable=False, server_default=sa.text("'{}'::jsonb")),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
        sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
        sa.UniqueConstraint("workspace_id", "test_run_id", name="uq_signal_verification_runs_workspace_run"),
    )
    op.create_index(
        "ix_signal_verification_runs_workspace_created",
        "signal_verification_runs",
        ["workspace_id", "created_at", "id"],
        unique=False,
    )
    op.create_index(
        "ix_signal_verification_runs_workspace_run",
        "signal_verification_runs",
        ["workspace_id", "test_run_id"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index("ix_signal_verification_runs_workspace_run", table_name="signal_verification_runs")
    op.drop_index("ix_signal_verification_runs_workspace_created", table_name="signal_verification_runs")
    op.drop_table("signal_verification_runs")
