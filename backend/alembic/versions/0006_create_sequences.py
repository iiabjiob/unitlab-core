"""create sequences and sequence_steps tables

Revision ID: 0006_create_sequences
Revises: 0005_create_switchgears
Create Date: 2025-09-14
"""
from typing import Sequence, Union
from alembic import op
import sqlalchemy as sa

# revision identifiers
revision: str = "0006_create_sequences"
down_revision: Union[str, None] = "0005_create_switchgears"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "sequences",
        sa.Column("id", sa.BigInteger, primary_key=True, autoincrement=True),
        sa.Column("name", sa.String, nullable=False),
        sa.Column("description", sa.Text, nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
    )

    op.create_table(
        "sequence_steps",
        sa.Column("id", sa.BigInteger, primary_key=True, autoincrement=True),
        sa.Column("sequence_id", sa.BigInteger, sa.ForeignKey("sequences.id", ondelete="CASCADE"), nullable=False),
        sa.Column("order_index", sa.Integer, nullable=False),
        sa.Column("kind", sa.String, nullable=False),  # e.g. DO_SET, WAIT
        sa.Column("unit_id", sa.String, nullable=True),
        sa.Column("payload", sa.JSON, nullable=True),  # ch, value, bitmask, pulse_ms, ms и т.д.
    )


def downgrade() -> None:
    op.drop_table("sequence_steps")
    op.drop_table("sequences")
