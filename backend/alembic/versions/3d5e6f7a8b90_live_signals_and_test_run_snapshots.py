"""introduce live signals and per-run snapshots

Revision ID: 3d5e6f7a8b90
Revises: 2b7c4a1d3e9f
Create Date: 2026-01-13 10:00:00.000000

"""
from __future__ import annotations

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import ENUM, JSONB

revision: str = "3d5e6f7a8b90"
down_revision: Union[str, None] = "2b7c4a1d3e9f"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")

signal_io_direction_enum = ENUM(
    "DI",
    "DO",
    "AI",
    "AO",
    name="signal_io_direction_enum",
    create_type=False,
)

signal_snapshot_status_enum = ENUM(
    "draft",
    "locked",
    name="signal_snapshot_status_enum",
    create_type=False,
)

test_run_mode_enum = ENUM(
    "channel",
    "signal",
    name="test_run_mode_enum",
    create_type=False,
)


def upgrade() -> None:
    bind = op.get_bind()
    signal_io_direction_enum.create(bind, checkfirst=True)

    op.create_table(
        "signals",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "workspace_id",
            BIGINT_PK,
            sa.ForeignKey("workspaces.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("key", sa.String(length=128), nullable=False),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column("io_direction", signal_io_direction_enum, nullable=False),
        sa.Column("category", sa.String(length=64), nullable=True),
        sa.Column(
            "metadata",
            JSONB,
            nullable=False,
            server_default=sa.text("'{}'::jsonb"),
        ),
        sa.Column(
            "is_active",
            sa.Boolean(),
            nullable=False,
            server_default=sa.text("true"),
        ),
        sa.Column("deleted_at", sa.DateTime(timezone=True), nullable=True),
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
        sa.UniqueConstraint("workspace_id", "key", name="uq_signals_workspace_key"),
    )
    op.create_index(
        "ix_signals_workspace",
        "signals",
        ["workspace_id"],
        unique=False,
    )
    op.create_index(
        "ix_signals_workspace_active",
        "signals",
        ["workspace_id", "is_active"],
        unique=False,
    )

    op.create_table(
        "test_run_signal_snapshots",
        sa.Column(
            "test_run_id",
            BIGINT_PK,
            sa.ForeignKey("test_runs.id", ondelete="CASCADE"),
            primary_key=True,
        ),
        sa.Column(
            "workspace_id",
            BIGINT_PK,
            sa.ForeignKey("workspaces.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column(
            "captured_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
    )
    op.create_index(
        "ix_test_run_signal_snapshots_workspace",
        "test_run_signal_snapshots",
        ["workspace_id"],
        unique=False,
    )

    op.create_table(
        "test_run_signal_snapshot_entries",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "snapshot_id",
            BIGINT_PK,
            sa.ForeignKey("test_run_signal_snapshots.test_run_id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column(
            "live_signal_id",
            BIGINT_PK,
            sa.ForeignKey("signals.id", ondelete="SET NULL"),
            nullable=True,
        ),
        sa.Column("signal_key", sa.String(length=128), nullable=False),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column("io_direction", signal_io_direction_enum, nullable=False),
        sa.Column(
            "allocation_channel_id",
            BIGINT_PK,
            sa.ForeignKey("channels.id", ondelete="SET NULL"),
            nullable=True,
        ),
        sa.Column("allocation_metadata", JSONB, nullable=True),
        sa.Column(
            "metadata",
            JSONB,
            nullable=False,
            server_default=sa.text("'{}'::jsonb"),
        ),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
    )
    op.create_index(
        "ix_snapshot_entries_snapshot",
        "test_run_signal_snapshot_entries",
        ["snapshot_id"],
        unique=False,
    )
    op.create_index(
        "ix_snapshot_entries_signal",
        "test_run_signal_snapshot_entries",
        ["live_signal_id"],
        unique=False,
    )

    op.add_column(
        "test_run_allocation_entries",
        sa.Column("signal_id", BIGINT_PK, nullable=True),
    )
    op.create_foreign_key(
        "fk_allocation_entries_signal",
        "test_run_allocation_entries",
        "signals",
        ["signal_id"],
        ["id"],
        ondelete="SET NULL",
    )

    op.drop_constraint("ck_test_runs_mode_snapshot", "test_runs", type_="check")
    op.drop_constraint("test_runs_signal_snapshot_id_fkey", "test_runs", type_="foreignkey")
    op.drop_column("test_runs", "mode")
    op.drop_column("test_runs", "signal_snapshot_id")

    op.drop_table("signal_snapshot_allocations")
    op.drop_index("ix_signal_snapshots_workspace", table_name="signal_snapshots")
    op.drop_table("signal_snapshots")
    signal_snapshot_status_enum.drop(bind, checkfirst=True)
    test_run_mode_enum.drop(bind, checkfirst=True)

    op.execute(
        """
        INSERT INTO test_run_signal_snapshots (test_run_id, workspace_id, captured_at)
        SELECT id, workspace_id, COALESCE(started_at, created_at)
        FROM test_runs
        """
    )


def downgrade() -> None:
    bind = op.get_bind()

    signal_snapshot_status_enum.create(bind, checkfirst=True)
    test_run_mode_enum.create(bind, checkfirst=True)

    op.create_table(
        "signal_snapshots",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "workspace_id",
            BIGINT_PK,
            sa.ForeignKey("workspaces.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("status", signal_snapshot_status_enum, nullable=False, server_default="draft"),
        sa.Column("source_filename", sa.String(length=255), nullable=True),
        sa.Column("source_hash", sa.String(length=64), nullable=True),
        sa.Column("rows_count", sa.Integer(), nullable=False, server_default=sa.text("0")),
        sa.Column("schema_version", sa.Integer(), nullable=False, server_default="1"),
        sa.Column("data", JSONB, nullable=False, server_default=sa.text("'{}'::jsonb")),
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
        "signal_snapshot_allocations",
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
        sa.UniqueConstraint("signal_snapshot_id", name="uq_snapshot_allocation_signal_snapshot"),
    )
    op.create_index(
        "ix_snapshot_allocations_workspace",
        "signal_snapshot_allocations",
        ["workspace_id"],
        unique=False,
    )

    op.add_column(
        "test_runs",
        sa.Column("signal_snapshot_id", BIGINT_PK, nullable=True),
    )
    op.create_foreign_key(
        "test_runs_signal_snapshot_id_fkey",
        "test_runs",
        "signal_snapshots",
        ["signal_snapshot_id"],
        ["id"],
        ondelete="RESTRICT",
    )
    op.add_column(
        "test_runs",
        sa.Column("mode", test_run_mode_enum, nullable=False, server_default="channel"),
    )
    op.create_check_constraint(
        "ck_test_runs_mode_snapshot",
        "test_runs",
        "(mode = 'channel' AND signal_snapshot_id IS NULL) OR "
        "(mode = 'signal' AND signal_snapshot_id IS NOT NULL)",
    )

    op.drop_constraint("fk_allocation_entries_signal", "test_run_allocation_entries", type_="foreignkey")
    op.drop_column("test_run_allocation_entries", "signal_id")

    op.drop_index("ix_snapshot_entries_signal", table_name="test_run_signal_snapshot_entries")
    op.drop_index("ix_snapshot_entries_snapshot", table_name="test_run_signal_snapshot_entries")
    op.drop_table("test_run_signal_snapshot_entries")

    op.drop_index(
        "ix_test_run_signal_snapshots_workspace",
        table_name="test_run_signal_snapshots",
    )
    op.drop_table("test_run_signal_snapshots")

    op.drop_index("ix_signals_workspace_active", table_name="signals")
    op.drop_index("ix_signals_workspace", table_name="signals")
    op.drop_table("signals")

    signal_io_direction_enum.drop(bind, checkfirst=True)