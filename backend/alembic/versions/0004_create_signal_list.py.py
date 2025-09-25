"""create signal_list tables

Revision ID: 0004_create_signal_list
Revises: 0003_create_event_logs
Create Date: 2025-09-10
"""
from typing import Sequence, Union
from alembic import op
import sqlalchemy as sa

# revision identifiers
revision: str = "0004_create_signal_list"
down_revision: Union[str, None] = "0003_create_event_logs"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "signal_list_revisions",
        sa.Column("id", sa.Integer, primary_key=True, autoincrement=True),
        sa.Column("project_name", sa.String(100), nullable=False),
        sa.Column("version", sa.String(50), nullable=False),
        sa.Column("uploaded_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
        sa.Column("description", sa.String(500), nullable=True),
        sa.Column("is_active", sa.Boolean, nullable=False, default=False),
    )

    op.create_table(
        "signal_list_entries",
        sa.Column("id", sa.Integer, primary_key=True, autoincrement=True),
        sa.Column("revision_id", sa.Integer, sa.ForeignKey("signal_list_revisions.id", ondelete="CASCADE"), nullable=False),
        sa.Column("unit_id", sa.String(100), nullable=True),
        sa.Column("channel_index", sa.Integer, nullable=True),
        sa.Column("terminal", sa.String(100), nullable=True),
        sa.Column("bay_name", sa.String(100), nullable=True),
        sa.Column("signal_name", sa.String(250), nullable=True),
        sa.Column("hmi_presentation_text", sa.String(500), nullable=True),
        sa.Column("signal_type", sa.String(10), nullable=True),
        sa.Column("group", sa.String(100), nullable=True),
        sa.Column("reaction_matrix", sa.String(100), nullable=True),
        sa.Column("external_address", sa.Integer, nullable=True),
        sa.Column("test_result", sa.String(10), nullable=True),
        sa.Column("tested_at", sa.DateTime(timezone=True), nullable=True),
    )


def downgrade() -> None:
    op.drop_table("signal_list_entries")
    op.drop_table("signal_list_revisions")
