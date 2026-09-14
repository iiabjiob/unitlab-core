"""persist signal-list revision provenance on test evidence"""

from alembic import op
import sqlalchemy as sa


revision = "cc2e3f4a5b6c"
down_revision = "cb1d2e3f4a5b"
branch_labels = None
depends_on = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def _backfill(table_name: str) -> None:
    op.execute(
        f"UPDATE {table_name} evidence "
        "SET signal_list_revision_id = ("
        "SELECT plan.revision_id FROM signal_test_run_plans plan "
        "WHERE plan.job_id = evidence.job_id) "
        "WHERE evidence.signal_list_revision_id IS NULL "
        "AND EXISTS (SELECT 1 FROM signal_test_run_plans plan "
        "WHERE plan.job_id = evidence.job_id)"
    )


def upgrade() -> None:
    op.add_column(
        "signal_test_run_step_evidence",
        sa.Column(
            "signal_list_revision_id",
            BIGINT_PK,
            sa.ForeignKey("signal_list_revisions.id", ondelete="RESTRICT"),
            nullable=True,
        ),
    )
    op.add_column(
        "signal_verification_evidence",
        sa.Column(
            "signal_list_revision_id",
            BIGINT_PK,
            sa.ForeignKey("signal_list_revisions.id", ondelete="RESTRICT"),
            nullable=True,
        ),
    )
    _backfill("signal_test_run_step_evidence")
    _backfill("signal_verification_evidence")
    op.create_index(
        "ix_signal_test_run_evidence_workspace_revision",
        "signal_test_run_step_evidence",
        ["workspace_id", "signal_list_revision_id", "created_at"],
        unique=False,
    )
    op.create_index(
        "ix_signal_verification_evidence_workspace_revision",
        "signal_verification_evidence",
        ["workspace_id", "signal_list_revision_id", "created_at"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index(
        "ix_signal_verification_evidence_workspace_revision",
        table_name="signal_verification_evidence",
    )
    op.drop_index(
        "ix_signal_test_run_evidence_workspace_revision",
        table_name="signal_test_run_step_evidence",
    )
    op.drop_column("signal_verification_evidence", "signal_list_revision_id")
    op.drop_column("signal_test_run_step_evidence", "signal_list_revision_id")
