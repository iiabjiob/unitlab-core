"""Remove legacy test run, allocation, and signal tables."""
from __future__ import annotations

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "8d3f6a21c4b5"
down_revision: Union[str, Sequence[str], None] = ("e5a1c2b3d4f5", "3d5e6f7a8b90")
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

TABLES_IN_DROP_ORDER = (
    "test_run_signal_snapshot_entries",
    "test_run_signal_snapshots",
    "test_run_allocation_entries",
    "test_run_allocations",
    "test_run_sequences",
    "test_run_steps",
    "test_runs",
    "signal_snapshot_allocations",
    "allocations",
    "signal_snapshots",
    "signals",
)

ENUM_TYPES = (
    "test_run_mode_enum",
    "test_run_status_enum",
    "signal_snapshot_status_enum",
    "signal_io_direction_enum",
)


def _drop_table_if_exists(table_name: str) -> None:
    inspector = sa.inspect(op.get_bind())
    if inspector.has_table(table_name):
        op.drop_table(table_name)


def _drop_enum_type(enum_name: str) -> None:
    bind = op.get_bind()
    if bind.dialect.name != "postgresql":
        return
    op.execute(sa.text(f"DROP TYPE IF EXISTS {enum_name} CASCADE"))


def upgrade() -> None:
    for table in TABLES_IN_DROP_ORDER:
        _drop_table_if_exists(table)

    for enum_name in ENUM_TYPES:
        _drop_enum_type(enum_name)


def downgrade() -> None:
    raise RuntimeError("downgrade not supported for removing legacy test run tables")
