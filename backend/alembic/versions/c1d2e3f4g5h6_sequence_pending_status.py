"""add pending status and widen active index

Revision ID: c1d2e3f4g5h6
Revises: ab12cd34ef56
Create Date: 2026-01-08 12:00:00.000000
"""

from alembic import op, context
import sqlalchemy as sa


def _add_enum_value(value: str) -> None:
    ctxt = context.get_context()
    with ctxt.autocommit_block():
        op.execute(
            sa.text(f"ALTER TYPE sequence_run_status_enum ADD VALUE IF NOT EXISTS '{value}'")
        )

revision = "c1d2e3f4g5h6"
down_revision = "ab12cd34ef56"
branch_labels = None
depends_on = None


def upgrade() -> None:
    _add_enum_value("pending")

    op.drop_constraint("ck_sequence_runs_status", "sequence_runs", type_="check")
    op.create_check_constraint(
        "ck_sequence_runs_status",
        "sequence_runs",
        "status IN ('pending','running','cancelling','completed','stopped','error')",
    )

    op.drop_index("uq_sequence_runs_active", table_name="sequence_runs")
    op.create_index(
        "uq_sequence_runs_active",
        "sequence_runs",
        ["sequence_id"],
        unique=True,
        postgresql_where=sa.text("status IN ('pending','running','cancelling')"),
    )

    op.alter_column(
        "sequence_runs",
        "status",
        server_default=sa.text("'pending'::sequence_run_status_enum"),
    )


def downgrade() -> None:
    op.drop_constraint("ck_sequence_runs_status", "sequence_runs", type_="check")
    op.create_check_constraint(
        "ck_sequence_runs_status",
        "sequence_runs",
        "status IN ('running','cancelling','completed','stopped','error')",
    )

    op.drop_index("uq_sequence_runs_active", table_name="sequence_runs")
    op.create_index(
        "uq_sequence_runs_active",
        "sequence_runs",
        ["sequence_id"],
        unique=True,
        postgresql_where=sa.text("status = 'running'"),
    )

    op.alter_column(
        "sequence_runs",
        "status",
        server_default=sa.text("'running'::sequence_run_status_enum"),
    )
    # PostgreSQL cannot easily drop enum values; downgrade keeps 'pending'.
