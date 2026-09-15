"""Allow blocked sequence steps in the persisted status check."""

from alembic import op


revision = "20260915_seq_step_check"
down_revision = "20260915_seq_partial_status"
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.drop_constraint(
        "ck_sequence_run_steps_status",
        "sequence_run_steps",
        type_="check",
    )
    op.create_check_constraint(
        "ck_sequence_run_steps_status",
        "sequence_run_steps",
        "status IN ('pending','running','completed','error','blocked','cancelled')",
    )


def downgrade() -> None:
    op.drop_constraint(
        "ck_sequence_run_steps_status",
        "sequence_run_steps",
        type_="check",
    )
    op.create_check_constraint(
        "ck_sequence_run_steps_status",
        "sequence_run_steps",
        "status IN ('pending','running','completed','error','cancelled')",
    )
