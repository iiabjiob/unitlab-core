"""Add system metadata columns to sequences

Revision ID: 2b7c4a1d3e9f
Revises: 1a3b5c7d9e00
Create Date: 2026-01-13 00:00:00.000000

"""
from __future__ import annotations

from alembic import op
import sqlalchemy as sa

revision: str = "2b7c4a1d3e9f"
down_revision: str | None = "1a3b5c7d9e00"
branch_labels = None
depends_on = None


def upgrade() -> None:
    with op.batch_alter_table("sequences") as batch:
        batch.add_column(sa.Column("system_key", sa.String(length=255), nullable=True))
        batch.add_column(
            sa.Column(
                "system_provided",
                sa.Boolean(),
                nullable=False,
                server_default=sa.false(),
            )
        )
        batch.add_column(
            sa.Column(
                "read_only",
                sa.Boolean(),
                nullable=False,
                server_default=sa.false(),
            )
        )
        batch.create_unique_constraint("uq_sequences_system_key", ["system_key"])

    op.execute("UPDATE sequences SET system_provided = FALSE WHERE system_provided IS NULL")
    op.execute("UPDATE sequences SET read_only = FALSE WHERE read_only IS NULL")


def downgrade() -> None:
    with op.batch_alter_table("sequences") as batch:
        batch.drop_constraint("uq_sequences_system_key", type_="unique")
        batch.drop_column("read_only")
        batch.drop_column("system_provided")
        batch.drop_column("system_key")
