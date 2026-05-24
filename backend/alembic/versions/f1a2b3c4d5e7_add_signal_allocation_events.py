"""add signal allocation events

Revision ID: f1a2b3c4d5e7
Revises: d1e2f3a4b5c6
Create Date: 2026-05-24 19:05:00.000000
"""

from alembic import op
import sqlalchemy as sa


revision = "f1a2b3c4d5e7"
down_revision = "d1e2f3a4b5c6"
branch_labels = None
depends_on = None


BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    op.create_table(
        "signal_allocation_events",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("operation", sa.String(length=32), nullable=False),
        sa.Column("source", sa.String(length=32), nullable=False, server_default="api"),
        sa.Column("signal_id", BIGINT_PK, nullable=True),
        sa.Column("previous_channel_id", BIGINT_PK, nullable=True),
        sa.Column("channel_id", BIGINT_PK, nullable=True),
        sa.Column("requested_count", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("changed_count", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("skipped_count", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("rejected_count", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("payload", sa.JSON(), nullable=False, server_default=sa.text("'{}'")),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
    )
    op.create_index(
        "ix_signal_allocation_events_workspace_created",
        "signal_allocation_events",
        ["workspace_id", "created_at", "id"],
        unique=False,
    )
    op.create_index(
        "ix_signal_allocation_events_workspace_operation",
        "signal_allocation_events",
        ["workspace_id", "operation", "created_at"],
        unique=False,
    )
    op.create_index(
        "ix_signal_allocation_events_workspace_signal",
        "signal_allocation_events",
        ["workspace_id", "signal_id", "created_at"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index("ix_signal_allocation_events_workspace_signal", table_name="signal_allocation_events")
    op.drop_index("ix_signal_allocation_events_workspace_operation", table_name="signal_allocation_events")
    op.drop_index("ix_signal_allocation_events_workspace_created", table_name="signal_allocation_events")
    op.drop_table("signal_allocation_events")
