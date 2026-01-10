"""add signal snapshots, allocations, and test runs

Revision ID: 10a2b3c4d5e6
Revises: 0c1d2e3f4a5b
Create Date: 2026-01-10 15:45:00.000000

"""
from __future__ import annotations

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import ENUM, JSONB

revision: str = "10a2b3c4d5e6"
down_revision: Union[str, None] = "0c1d2e3f4a5b"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")

SIGNAL_SNAPSHOT_STATUS_VALUES = ("draft", "locked")
TEST_RUN_STATUS_VALUES = ("created", "running", "completed", "failed")

signal_snapshot_status_enum = ENUM(
    *SIGNAL_SNAPSHOT_STATUS_VALUES,
    name="signal_snapshot_status_enum",
    create_type=True,
)
signal_snapshot_status_enum_no_create = ENUM(
    *SIGNAL_SNAPSHOT_STATUS_VALUES,
    name="signal_snapshot_status_enum",
    create_type=False,
)

test_run_status_enum = ENUM(
    *TEST_RUN_STATUS_VALUES,
    name="test_run_status_enum",
    create_type=True,
)
test_run_status_enum_no_create = ENUM(
    *TEST_RUN_STATUS_VALUES,
    name="test_run_status_enum",
    create_type=False,
)


def upgrade() -> None:
    bind = op.get_bind()
    signal_snapshot_status_enum.create(bind, checkfirst=True)
    test_run_status_enum.create(bind, checkfirst=True)

    op.create_table(
        "signal_snapshots",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "workspace_id",
            BIGINT_PK,
            sa.ForeignKey("workspaces.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column(
            "status",
            signal_snapshot_status_enum_no_create,
            nullable=False,
            server_default="draft",
        ),
        sa.Column("source_filename", sa.String(length=255), nullable=True),
        sa.Column("source_hash", sa.String(length=64), nullable=True),
        sa.Column("rows_count", sa.Integer(), nullable=False),
        sa.Column(
            "schema_version",
            sa.Integer(),
            nullable=False,
            server_default="1",
        ),
        sa.Column("data", JSONB, nullable=False),
        sa.Column("locked_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.Column(
            "updated_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
    )
    op.create_index(
        "ix_signal_snapshots_workspace",
        "signal_snapshots",
        ["workspace_id"],
        unique=False,
    )

    op.create_table(
        "allocations",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "workspace_id",
            BIGINT_PK,
            sa.ForeignKey("workspaces.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column(
            "signal_snapshot_id",
            BIGINT_PK,
            sa.ForeignKey("signal_snapshots.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("mapping", JSONB, nullable=False, server_default=sa.text("'[]'::jsonb")),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.Column(
            "updated_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.UniqueConstraint("signal_snapshot_id", name="uq_allocations_snapshot"),
    )
    op.create_index(
        "ix_allocations_workspace",
        "allocations",
        ["workspace_id"],
        unique=False,
    )

    op.create_table(
        "test_runs",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "workspace_id",
            BIGINT_PK,
            sa.ForeignKey("workspaces.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column(
            "sequence_id",
            BIGINT_PK,
            sa.ForeignKey("sequences.id", ondelete="RESTRICT"),
            nullable=False,
        ),
        sa.Column(
            "signal_snapshot_id",
            BIGINT_PK,
            sa.ForeignKey("signal_snapshots.id", ondelete="RESTRICT"),
            nullable=False,
        ),
        sa.Column("allocation_snapshot", JSONB, nullable=False),
        sa.Column(
            "status",
            test_run_status_enum_no_create,
            nullable=False,
            server_default="created",
        ),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.Column("started_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("finished_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("execution_meta", JSONB, nullable=True),
    )
    op.create_index(
        "ix_test_runs_workspace_created",
        "test_runs",
        ["workspace_id", "created_at"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index("ix_test_runs_workspace_created", table_name="test_runs")
    op.drop_table("test_runs")

    op.drop_index("ix_allocations_workspace", table_name="allocations")
    op.drop_table("allocations")

    op.drop_index("ix_signal_snapshots_workspace", table_name="signal_snapshots")
    op.drop_table("signal_snapshots")

    bind = op.get_bind()
    test_run_status_enum.drop(bind, checkfirst=True)
    signal_snapshot_status_enum.drop(bind, checkfirst=True)
