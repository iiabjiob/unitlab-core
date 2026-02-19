"""add signals tested_at column

Revision ID: b8e1d2c3f4a6
Revises: a4b6c8d0e1f2
Create Date: 2026-02-19 15:30:00.000000
"""

from alembic import op
import sqlalchemy as sa


revision = "b8e1d2c3f4a6"
down_revision = "a4b6c8d0e1f2"
branch_labels = None
depends_on = None


ISO_TS_REGEX = r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\\.[0-9]{1,6})?(Z|[+-][0-9]{2}:[0-9]{2})?$"


def upgrade() -> None:
    op.add_column("signals", sa.Column("tested_at", sa.DateTime(timezone=True), nullable=True))
    op.create_index("ix_signals_workspace_tested_at", "signals", ["workspace_id", "tested_at"], unique=False)

    op.execute(
        f"""
        UPDATE signals
        SET tested_at = REPLACE(metadata->>'tested_at', 'Z', '+00:00')::timestamptz
        WHERE tested_at IS NULL
          AND COALESCE(metadata->>'tested_at', '') ~ '{ISO_TS_REGEX}'
        """
    )

    op.execute(
        f"""
        UPDATE signals
        SET tested_at = REPLACE(metadata->>'testedAt', 'Z', '+00:00')::timestamptz
        WHERE tested_at IS NULL
          AND COALESCE(metadata->>'testedAt', '') ~ '{ISO_TS_REGEX}'
        """
    )


def downgrade() -> None:
    op.drop_index("ix_signals_workspace_tested_at", table_name="signals")
    op.drop_column("signals", "tested_at")
