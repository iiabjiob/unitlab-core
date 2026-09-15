"""Allow partial completion in the persisted sequence run status check."""

from alembic import op


revision = "20260915_seq_run_check"
down_revision = "20260915_seq_step_check"
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.drop_constraint(
        "ck_sequence_runs_status",
        "sequence_runs",
        type_="check",
    )
    op.create_check_constraint(
        "ck_sequence_runs_status",
        "sequence_runs",
        "status IN ('pending','running','cancelling','completed','completed_with_issues','stopped','error')",
    )


def downgrade() -> None:
    op.drop_constraint(
        "ck_sequence_runs_status",
        "sequence_runs",
        type_="check",
    )
    op.create_check_constraint(
        "ck_sequence_runs_status",
        "sequence_runs",
        "status IN ('pending','running','cancelling','completed','stopped','error')",
    )
