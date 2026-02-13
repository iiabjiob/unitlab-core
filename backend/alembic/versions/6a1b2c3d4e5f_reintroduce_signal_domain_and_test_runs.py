"""reintroduce signal domain and test runs

Revision ID: 6a1b2c3d4e5f
Revises: 8d3f6a21c4b5
Create Date: 2026-02-13 10:30:00.000000
"""

from __future__ import annotations

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "6a1b2c3d4e5f"
down_revision: Union[str, None] = "8d3f6a21c4b5"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")

signal_snapshot_status_enum = sa.Enum(
    "draft",
    "locked",
    name="signal_snapshot_status_enum",
    native_enum=False,
)

signal_io_direction_enum = sa.Enum(
    "DI",
    "DO",
    "AI",
    "AO",
    name="signal_io_direction_enum",
    native_enum=False,
)

test_run_status_enum = sa.Enum(
    "created",
    "running",
    "completed",
    "failed",
    name="test_run_status_enum",
    native_enum=False,
)


def upgrade() -> None:
    op.create_table(
        "signal_snapshots",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("status", signal_snapshot_status_enum, nullable=False, server_default="draft"),
        sa.Column("source_filename", sa.String(length=255), nullable=True),
        sa.Column("source_hash", sa.String(length=64), nullable=True),
        sa.Column("rows_count", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("schema_version", sa.Integer(), nullable=False, server_default="2"),
        sa.Column("data", sa.JSON(), nullable=False),
        sa.Column("import_meta", sa.JSON(), nullable=True),
        sa.Column("locked_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
    )
    op.create_index("ix_signal_snapshots_workspace", "signal_snapshots", ["workspace_id"], unique=False)
    op.create_index(
        "ix_signal_snapshots_workspace_updated",
        "signal_snapshots",
        ["workspace_id", "updated_at"],
        unique=False,
    )

    op.create_table(
        "signal_snapshot_allocations",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column(
            "signal_snapshot_id",
            BIGINT_PK,
            sa.ForeignKey("signal_snapshots.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("mapping", sa.JSON(), nullable=False, server_default="[]"),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.UniqueConstraint("signal_snapshot_id", name="uq_signal_snapshot_allocations_snapshot"),
    )
    op.create_index(
        "ix_signal_snapshot_allocations_workspace",
        "signal_snapshot_allocations",
        ["workspace_id"],
        unique=False,
    )

    op.create_table(
        "signals",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("key", sa.String(length=128), nullable=False),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column("io_direction", signal_io_direction_enum, nullable=False),
        sa.Column("category", sa.String(length=64), nullable=True),
        sa.Column("metadata", sa.JSON(), nullable=False, server_default="{}"),
        sa.Column("is_active", sa.Boolean(), nullable=False, server_default=sa.text("true")),
        sa.Column("deleted_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.UniqueConstraint("workspace_id", "key", name="uq_signals_workspace_key"),
    )
    op.create_index("ix_signals_workspace", "signals", ["workspace_id"], unique=False)
    op.create_index("ix_signals_workspace_active", "signals", ["workspace_id", "is_active"], unique=False)

    op.create_table(
        "test_runs",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("status", test_run_status_enum, nullable=False, server_default="created"),
        sa.Column("execution_meta", sa.JSON(), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("started_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("finished_at", sa.DateTime(timezone=True), nullable=True),
    )
    op.create_index("ix_test_runs_workspace_created", "test_runs", ["workspace_id", "created_at"], unique=False)
    op.create_index("ix_test_runs_workspace_status", "test_runs", ["workspace_id", "status"], unique=False)

    op.create_table(
        "test_run_sequences",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("test_run_id", BIGINT_PK, sa.ForeignKey("test_runs.id", ondelete="CASCADE"), nullable=False),
        sa.Column("sequence_id", BIGINT_PK, sa.ForeignKey("sequences.id", ondelete="RESTRICT"), nullable=False),
        sa.Column("order_index", sa.Integer(), nullable=False),
        sa.UniqueConstraint("test_run_id", "order_index", name="uq_test_run_sequences_order"),
        sa.UniqueConstraint("test_run_id", "sequence_id", name="uq_test_run_sequences_pair"),
    )
    op.create_index("ix_test_run_sequences_test_run", "test_run_sequences", ["test_run_id"], unique=False)
    op.create_index("ix_test_run_sequences_sequence", "test_run_sequences", ["sequence_id"], unique=False)

    op.create_table(
        "test_run_allocations",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("test_run_id", BIGINT_PK, sa.ForeignKey("test_runs.id", ondelete="CASCADE"), nullable=False),
        sa.Column("notes", sa.Text(), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.UniqueConstraint("test_run_id", name="uq_test_run_allocations_test_run"),
    )

    op.create_table(
        "test_run_allocation_entries",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "allocation_id",
            BIGINT_PK,
            sa.ForeignKey("test_run_allocations.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("channel_id", BIGINT_PK, sa.ForeignKey("channels.id", ondelete="RESTRICT"), nullable=False),
        sa.Column("signal_id", BIGINT_PK, sa.ForeignKey("signals.id", ondelete="SET NULL"), nullable=True),
        sa.Column("signal_metadata", sa.JSON(), nullable=True),
        sa.UniqueConstraint("allocation_id", "channel_id", name="uq_allocation_entry_channel"),
    )
    op.create_index(
        "ix_test_run_allocation_entries_channel",
        "test_run_allocation_entries",
        ["channel_id"],
        unique=False,
    )
    op.create_index(
        "ix_test_run_allocation_entries_signal",
        "test_run_allocation_entries",
        ["signal_id"],
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
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("captured_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
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
        sa.Column("live_signal_id", BIGINT_PK, sa.ForeignKey("signals.id", ondelete="SET NULL"), nullable=True),
        sa.Column("signal_key", sa.String(length=128), nullable=False),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column("io_direction", signal_io_direction_enum, nullable=False),
        sa.Column(
            "allocation_channel_id",
            BIGINT_PK,
            sa.ForeignKey("channels.id", ondelete="SET NULL"),
            nullable=True,
        ),
        sa.Column("allocation_metadata", sa.JSON(), nullable=True),
        sa.Column("metadata", sa.JSON(), nullable=False, server_default="{}"),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
    )
    op.create_index(
        "ix_test_run_signal_snapshot_entries_snapshot",
        "test_run_signal_snapshot_entries",
        ["snapshot_id"],
        unique=False,
    )
    op.create_index(
        "ix_test_run_signal_snapshot_entries_signal",
        "test_run_signal_snapshot_entries",
        ["live_signal_id"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index("ix_test_run_signal_snapshot_entries_signal", table_name="test_run_signal_snapshot_entries")
    op.drop_index("ix_test_run_signal_snapshot_entries_snapshot", table_name="test_run_signal_snapshot_entries")
    op.drop_table("test_run_signal_snapshot_entries")

    op.drop_index("ix_test_run_signal_snapshots_workspace", table_name="test_run_signal_snapshots")
    op.drop_table("test_run_signal_snapshots")

    op.drop_index("ix_test_run_allocation_entries_signal", table_name="test_run_allocation_entries")
    op.drop_index("ix_test_run_allocation_entries_channel", table_name="test_run_allocation_entries")
    op.drop_table("test_run_allocation_entries")

    op.drop_table("test_run_allocations")

    op.drop_index("ix_test_run_sequences_sequence", table_name="test_run_sequences")
    op.drop_index("ix_test_run_sequences_test_run", table_name="test_run_sequences")
    op.drop_table("test_run_sequences")

    op.drop_index("ix_test_runs_workspace_status", table_name="test_runs")
    op.drop_index("ix_test_runs_workspace_created", table_name="test_runs")
    op.drop_table("test_runs")

    op.drop_index("ix_signals_workspace_active", table_name="signals")
    op.drop_index("ix_signals_workspace", table_name="signals")
    op.drop_table("signals")

    op.drop_index("ix_signal_snapshot_allocations_workspace", table_name="signal_snapshot_allocations")
    op.drop_table("signal_snapshot_allocations")

    op.drop_index("ix_signal_snapshots_workspace_updated", table_name="signal_snapshots")
    op.drop_index("ix_signal_snapshots_workspace", table_name="signal_snapshots")
    op.drop_table("signal_snapshots")
