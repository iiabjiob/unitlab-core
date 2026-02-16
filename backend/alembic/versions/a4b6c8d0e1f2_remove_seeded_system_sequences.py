"""remove seeded system sequences

Revision ID: a4b6c8d0e1f2
Revises: d7e8f9a0b1c2, 9a1b2c3d4e6f
Create Date: 2026-02-16 10:20:00.000000
"""

from alembic import op


revision = "a4b6c8d0e1f2"
down_revision = ("d7e8f9a0b1c2", "9a1b2c3d4e6f")
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.execute(
        """
        DELETE FROM sequences
        WHERE system_provided IS TRUE
          AND read_only IS TRUE
          AND system_key IS NOT NULL
        """
    )


def downgrade() -> None:
    # Deleted seeded sequences are intentionally not restored.
    pass
