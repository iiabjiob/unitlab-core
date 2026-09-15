"""Add partial sequence execution statuses."""

from alembic import op


revision = "20260915_seq_partial_status"
down_revision = "20260914_initial_schema"
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.execute("ALTER TYPE sequence_run_status_enum ADD VALUE IF NOT EXISTS 'completed_with_issues'")
    op.execute("ALTER TYPE sequence_run_step_status_enum ADD VALUE IF NOT EXISTS 'blocked'")


def downgrade() -> None:
    # PostgreSQL enums cannot safely remove values in-place.
    pass
