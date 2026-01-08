"""ensure single running sequence per sequence_id

Revision ID: f6a7b8c9d0e1
Revises: e5a1c2b3d4f5
Create Date: 2026-01-08 00:00:00.000000
"""

from alembic import op
import sqlalchemy as sa

# revision identifiers, used by Alembic.
revision = "f6a7b8c9d0e1"
down_revision = "e5a1c2b3d4f5"
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.create_index(
        "uq_sequence_runs_active",
        "sequence_runs",
        ["sequence_id"],
        unique=True,
        postgresql_where=sa.text("status = 'running'"),
    )


def downgrade() -> None:
    op.drop_index("uq_sequence_runs_active", table_name="sequence_runs")
