"""create devices table

Revision ID: 0001_create_devices
Revises: 
Create Date: 2025-09-10
"""
from typing import Sequence, Union
from alembic import op
import sqlalchemy as sa

# revision identifiers
revision: str = "0001_create_devices"
down_revision: Union[str, None] = None
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "devices",
        sa.Column("id", sa.Integer, primary_key=True, autoincrement=True),
        sa.Column("unit_id", sa.String, unique=True, index=True, nullable=False),
        sa.Column("type", sa.String, nullable=True),
        sa.Column("num_channels", sa.Integer, nullable=True),
        sa.Column("firmware_version", sa.String, nullable=True),
        sa.Column("name", sa.String, nullable=True),
        sa.Column("location", sa.String, nullable=True),
        sa.Column("is_active", sa.Boolean, default=True, nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now()),
    )


def downgrade() -> None:
    op.drop_table("devices")
