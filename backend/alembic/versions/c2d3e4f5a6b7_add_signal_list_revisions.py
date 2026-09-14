"""add durable signal list revisions

Revision ID: c2d3e4f5a6b7
Revises: b4c5d6e7f8a9
"""

from alembic import op
import sqlalchemy as sa


revision = "c2d3e4f5a6b7"
down_revision = "b4c5d6e7f8a9"
branch_labels = None
depends_on = None
BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    op.create_table(
        "signal_list_revisions",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("revision_no", sa.Integer(), nullable=False),
        sa.Column("status", sa.String(16), nullable=False, server_default="active"),
        sa.Column("source_hash", sa.String(64), nullable=True),
        sa.Column("schema_version", sa.Integer(), nullable=False, server_default="1"),
        sa.Column("rows_count", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("content_hash", sa.String(64), nullable=False),
        sa.Column("created_by", sa.String(128), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("activated_at", sa.DateTime(timezone=True), nullable=True),
        sa.UniqueConstraint("workspace_id", "revision_no", name="uq_signal_list_revisions_workspace_no"),
    )
    op.create_index("ix_signal_list_revisions_workspace_status", "signal_list_revisions", ["workspace_id", "status", "created_at"])
    op.create_table(
        "signal_list_revision_items",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("revision_id", BIGINT_PK, sa.ForeignKey("signal_list_revisions.id", ondelete="CASCADE"), nullable=False),
        sa.Column("live_signal_id", BIGINT_PK, sa.ForeignKey("signals.id", ondelete="SET NULL"), nullable=True),
        sa.Column("order_index", sa.Integer(), nullable=False),
        sa.Column("signal_key", sa.String(128), nullable=False),
        sa.Column("snapshot", sa.JSON(), nullable=False, server_default=sa.text("'{}'")),
        sa.UniqueConstraint("revision_id", "order_index", name="uq_signal_list_revision_items_order"),
    )
    op.create_index("ix_signal_list_revision_items_revision", "signal_list_revision_items", ["revision_id", "order_index"])


def downgrade() -> None:
    op.drop_index("ix_signal_list_revision_items_revision", table_name="signal_list_revision_items")
    op.drop_table("signal_list_revision_items")
    op.drop_index("ix_signal_list_revisions_workspace_status", table_name="signal_list_revisions")
    op.drop_table("signal_list_revisions")
