"""add cancelling status for sequence runs

Revision ID: ab12cd34ef56
Revises: f6a7b8c9d0e1
Create Date: 2026-01-08 00:10:00.000000
"""

from alembic import op, context
import sqlalchemy as sa


def _add_enum_value(value: str) -> None:
    ctxt = context.get_context()
    with ctxt.autocommit_block():
        op.execute(
            sa.text(f"ALTER TYPE sequence_run_status_enum ADD VALUE IF NOT EXISTS '{value}'")
        )

revision = "ab12cd34ef56"
down_revision = "f6a7b8c9d0e1"
branch_labels = None
depends_on = None


def upgrade() -> None:
    # Extend underlying enum type (PostgreSQL specific)
    _add_enum_value("cancelling")

    op.drop_constraint("ck_sequence_runs_status", "sequence_runs", type_="check")
    op.create_check_constraint(
        "ck_sequence_runs_status",
        "sequence_runs",
        "status IN ('running','cancelling','completed','stopped','error')",
    )


def downgrade() -> None:
    op.drop_constraint("ck_sequence_runs_status", "sequence_runs", type_="check")
    op.create_check_constraint(
        "ck_sequence_runs_status",
        "sequence_runs",
        "status IN ('running','completed','stopped','error')",
    )
    # PostgreSQL cannot remove enum values easily; downgrade keeps the value.
