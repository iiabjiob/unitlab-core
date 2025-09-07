"""create event_log table

Revision ID: 0002_create_event_log
Revises: 0001_create_devices
Create Date: 2025-09-07
"""
from typing import Sequence, Union
from alembic import op
import sqlalchemy as sa

# revision identifiers
revision: str = "0002_create_event_log"
down_revision: Union[str, None] = "0001_create_devices"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "event_log",
        sa.Column("id", sa.String, primary_key=True, index=True),
        sa.Column("ts", sa.BigInteger, nullable=False),
        sa.Column("dir", sa.String, nullable=False),
        sa.Column("source", sa.String, nullable=False),
        sa.Column("channel_or_action", sa.String, nullable=False),
        sa.Column("unit_id", sa.String, nullable=True),
        sa.Column("type", sa.String, nullable=True),
        sa.Column("summary", sa.String, nullable=False),
        sa.Column("payload", sa.JSON, nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now()),
    )


def downgrade() -> None:
    op.drop_table("event_log")
