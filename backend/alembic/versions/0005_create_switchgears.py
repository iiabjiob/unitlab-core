"""create switchgear table

Revision ID: 0005_create_switchgears
Revises: 0004_create_signal_list
Create Date: 2025-09-12
"""
from typing import Sequence, Union
from alembic import op
import sqlalchemy as sa

# revision identifiers
revision: str = "0005_create_switchgears"
down_revision: Union[str, None] = "0004_create_signal_list"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "switchgears",
        sa.Column("id", sa.BigInteger, primary_key=True, autoincrement=True),
        sa.Column("title", sa.String, nullable=False),
        sa.Column("kind", sa.String, nullable=False, server_default="switchgear"),

        sa.Column("do_open", sa.Integer, sa.ForeignKey("channels.id", ondelete="SET NULL"), nullable=True),
        sa.Column("do_closed", sa.Integer, sa.ForeignKey("channels.id", ondelete="SET NULL"), nullable=True),
        sa.Column("di_open", sa.Integer, sa.ForeignKey("channels.id", ondelete="SET NULL"), nullable=True),
        sa.Column("di_close", sa.Integer, sa.ForeignKey("channels.id", ondelete="SET NULL"), nullable=True),

        sa.Column("feedback_delay_ms", sa.Integer, nullable=False, server_default="0"),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now()),
    )


def downgrade() -> None:
    op.drop_table("switchgears")
