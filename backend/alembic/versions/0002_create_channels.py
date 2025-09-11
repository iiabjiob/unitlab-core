"""create channels table

Revision ID: 0002_create_channels
Revises: 0001_create_devices
Create Date: 2025-09-10
"""
from typing import Sequence, Union
from alembic import op
import sqlalchemy as sa

# revision identifiers
revision: str = "0002_create_channels"
down_revision: Union[str, None] = "0001_create_devices"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "channels",
        sa.Column("id", sa.Integer, primary_key=True, autoincrement=True),
        sa.Column("device_id", sa.Integer, sa.ForeignKey("devices.id", ondelete="CASCADE"), nullable=False),
        sa.Column("index", sa.Integer, nullable=False),  # канал внутри устройства
        sa.Column("type", sa.String, nullable=False),    # DO, DI, AO ...
        sa.Column("name", sa.String, nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now()),
    )

    # уникальность канала внутри устройства
    op.create_unique_constraint(
        "uq_channels_device_index", "channels", ["device_id", "index"]
    )


def downgrade() -> None:
    op.drop_table("channels")
