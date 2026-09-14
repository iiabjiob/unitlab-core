"""enforce one active signal-list revision per workspace"""

from alembic import op
import sqlalchemy as sa


revision = "cb1d2e3f4a5b"
down_revision = "ca9b8c7d6e5f"
branch_labels = None
depends_on = None


def upgrade() -> None:
    # Keep the newest active revision before installing the database invariant.
    op.execute(
        "UPDATE signal_list_revisions older "
        "SET status = 'archived' "
        "WHERE older.status = 'active' AND EXISTS ("
        "SELECT 1 FROM signal_list_revisions newer "
        "WHERE newer.workspace_id = older.workspace_id "
        "AND newer.status = 'active' AND newer.id > older.id)"
    )
    op.create_index(
        "uq_signal_list_revisions_workspace_active",
        "signal_list_revisions",
        ["workspace_id"],
        unique=True,
        postgresql_where=sa.text("status = 'active'"),
        sqlite_where=sa.text("status = 'active'"),
    )


def downgrade() -> None:
    op.drop_index(
        "uq_signal_list_revisions_workspace_active",
        table_name="signal_list_revisions",
    )
