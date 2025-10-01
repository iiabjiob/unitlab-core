"""create event_logs table

Revision ID: 0003_create_event_logs
Revises: 0002_create_channels
Create Date: 2025-09-10
"""
from typing import Sequence, Union
from alembic import op
import sqlalchemy as sa

# revision identifiers
revision: str = "0003_create_event_logs"
down_revision: Union[str, None] = "0002_create_channels"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "event_logs",
        sa.Column("id", sa.BigInteger, primary_key=True, autoincrement=True),
        sa.Column("ts", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
        sa.Column("type", sa.String, nullable=False),  # тип события: DO_SET, AO_UPDATE, SEQUENCE_START ...
        sa.Column("summary", sa.String, nullable=False),  # короткое описание
        sa.Column("payload", sa.JSON, nullable=True),  # детали события

        sa.Column("channel_id", sa.BigInteger, sa.ForeignKey("channels.id", ondelete="SET NULL"), nullable=True),

        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
    )


def downgrade() -> None:
    op.drop_table("event_logs")
