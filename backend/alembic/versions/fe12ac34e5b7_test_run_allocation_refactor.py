"""refactor test run allocation domain

Revision ID: fe12ac34e5b7
Revises: ba83d9592bb6
Create Date: 2026-01-11 12:10:00.000000

"""
from __future__ import annotations

from collections import defaultdict
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import ENUM, JSONB


# revision identifiers, used by Alembic.
revision: str = "fe12ac34e5b7"
down_revision: Union[str, None] = "ba83d9592bb6"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")
TEST_RUN_MODE_VALUES = ("channel", "signal")

test_run_mode_enum = ENUM(
    *TEST_RUN_MODE_VALUES,
    name="test_run_mode_enum",
    create_type=True,
)
test_run_mode_enum_no_create = ENUM(
    *TEST_RUN_MODE_VALUES,
    name="test_run_mode_enum",
    create_type=False,
)

test_run_sequences_table = sa.table(
    "test_run_sequences",
    sa.column("test_run_id", BIGINT_PK),
    sa.column("sequence_id", BIGINT_PK),
)

test_run_allocation_entries_table = sa.table(
    "test_run_allocation_entries",
    sa.column("allocation_id", BIGINT_PK),
    sa.column("channel_id", BIGINT_PK),
    sa.column("signal_key", sa.String()),
    sa.column("signal_metadata", JSONB),
)


def upgrade() -> None:
    bind = op.get_bind()
    test_run_mode_enum.create(bind, checkfirst=True)

    op.add_column(
        "test_runs",
        sa.Column(
            "mode",
            test_run_mode_enum_no_create,
            nullable=False,
            server_default="channel",
        ),
    )
    op.alter_column(
        "test_runs",
        "signal_snapshot_id",
        existing_type=BIGINT_PK,
        nullable=True,
    )
    op.execute("UPDATE test_runs SET mode = 'signal' WHERE signal_snapshot_id IS NOT NULL")
    op.create_check_constraint(
        "ck_test_runs_mode_snapshot",
        "test_runs",
        "(mode = 'channel' AND signal_snapshot_id IS NULL) OR "
        "(mode = 'signal' AND signal_snapshot_id IS NOT NULL)",
    )

    op.create_table(
        "test_run_sequences",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "test_run_id",
            BIGINT_PK,
            sa.ForeignKey("test_runs.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column(
            "sequence_id",
            BIGINT_PK,
            sa.ForeignKey("sequences.id", ondelete="RESTRICT"),
            nullable=False,
        ),
        sa.UniqueConstraint("test_run_id", "sequence_id", name="uq_test_run_sequence_pair"),
    )
    op.create_index(
        "ix_test_run_sequences_test_run",
        "test_run_sequences",
        ["test_run_id"],
        unique=False,
    )
    op.create_index(
        "ix_test_run_sequences_sequence",
        "test_run_sequences",
        ["sequence_id"],
        unique=False,
    )

    op.create_table(
        "test_run_allocations",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "test_run_id",
            BIGINT_PK,
            sa.ForeignKey("test_runs.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("notes", sa.Text(), nullable=True),
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
            server_onupdate=sa.text("now()"),
            nullable=False,
        ),
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
        sa.Column(
            "channel_id",
            BIGINT_PK,
            sa.ForeignKey("channels.id", ondelete="RESTRICT"),
            nullable=False,
        ),
        sa.Column("signal_key", sa.String(), nullable=True),
        sa.Column("signal_metadata", JSONB, nullable=True),
        sa.UniqueConstraint("allocation_id", "channel_id", name="uq_allocation_entry_channel"),
    )
    op.create_index(
        "ix_allocation_entries_channel",
        "test_run_allocation_entries",
        ["channel_id"],
        unique=False,
    )

    rows = bind.execute(
        sa.text(
            "SELECT id, sequence_id, allocation_snapshot, created_at "
            "FROM test_runs ORDER BY id"
        )
    ).mappings().all()

    if rows:
        sequence_rows = [
            {"test_run_id": row["id"], "sequence_id": row["sequence_id"]}
            for row in rows
            if row["sequence_id"] is not None
        ]
        if sequence_rows:
            op.bulk_insert(test_run_sequences_table, sequence_rows)

        allocation_insert_stmt = sa.text(
            """
            INSERT INTO test_run_allocations (test_run_id, notes, created_at, updated_at)
            VALUES (:test_run_id, :notes, :created_at, :updated_at)
            RETURNING id
            """
        )

        for row in rows:
            allocation_id = bind.execute(
                allocation_insert_stmt,
                {
                    "test_run_id": row["id"],
                    "notes": None,
                    "created_at": row["created_at"],
                    "updated_at": row["created_at"],
                },
            ).scalar_one()
            snapshot_entries = row["allocation_snapshot"] or []
            entry_rows = []
            if isinstance(snapshot_entries, list):
                for entry in snapshot_entries:
                    if not isinstance(entry, dict):
                        continue
                    channel_id = entry.get("channel_id")
                    try:
                        channel_id_int = int(channel_id) if channel_id is not None else None
                    except (TypeError, ValueError):
                        channel_id_int = None
                    if channel_id_int is None:
                        continue
                    entry_rows.append(
                        {
                            "allocation_id": allocation_id,
                            "channel_id": channel_id_int,
                            "signal_key": entry.get("signal_key"),
                            "signal_metadata": entry.get("signal_metadata"),
                        }
                    )
            if entry_rows:
                bind.execute(test_run_allocation_entries_table.insert(), entry_rows)

    op.drop_column("test_runs", "sequence_id")
    op.drop_column("test_runs", "allocation_snapshot")

    op.drop_table("allocations")


def downgrade() -> None:
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

    op.add_column(
        "test_runs",
        sa.Column("sequence_id", BIGINT_PK, nullable=True),
    )
    op.add_column(
        "test_runs",
        sa.Column(
            "allocation_snapshot",
            JSONB,
            nullable=False,
            server_default=sa.text("'[]'::jsonb"),
        ),
    )

    bind = op.get_bind()

    sequence_rows = bind.execute(
        sa.text(
            "SELECT test_run_id, sequence_id FROM test_run_sequences ORDER BY id"
        )
    ).mappings().all()
    first_sequence_per_run: dict[int, int] = {}
    for row in sequence_rows:
        run_id = row["test_run_id"]
        if run_id not in first_sequence_per_run:
            first_sequence_per_run[run_id] = row["sequence_id"]

    for run_id, sequence_id in first_sequence_per_run.items():
        bind.execute(
            sa.text(
                "UPDATE test_runs SET sequence_id = :sequence_id WHERE id = :run_id"
            ),
            {"sequence_id": sequence_id, "run_id": run_id},
        )

    missing_sequences = bind.execute(
        sa.text("SELECT COUNT(*) FROM test_runs WHERE sequence_id IS NULL")
    ).scalar_one()
    if missing_sequences:
        raise RuntimeError("Cannot downgrade: some test runs are missing sequence assignments")

    op.alter_column(
        "test_runs",
        "sequence_id",
        existing_type=BIGINT_PK,
        nullable=False,
    )

    allocation_rows = bind.execute(
        sa.text(
            "SELECT tra.test_run_id, entry.channel_id, entry.signal_key, entry.signal_metadata "
            "FROM test_run_allocations tra "
            "LEFT JOIN test_run_allocation_entries entry ON entry.allocation_id = tra.id "
            "ORDER BY tra.test_run_id, entry.id"
        )
    ).mappings().all()
    payloads: dict[int, list[dict[str, object]]] = defaultdict(list)
    for row in allocation_rows:
        channel_id = row["channel_id"]
        if channel_id is None:
            continue
        payload: dict[str, object] = {"channel_id": int(channel_id)}
        if row["signal_key"] is not None:
            payload["signal_key"] = row["signal_key"]
        if row["signal_metadata"] is not None:
            payload["signal_metadata"] = row["signal_metadata"]
        payloads[row["test_run_id"]].append(payload)

    run_ids = bind.execute(sa.text("SELECT id FROM test_runs")).scalars().all()
    for run_id in run_ids:
        bind.execute(
            sa.text("UPDATE test_runs SET allocation_snapshot = :payload WHERE id = :run_id"),
            {"payload": payloads.get(run_id, []), "run_id": run_id},
        )

    op.alter_column(
        "test_runs",
        "allocation_snapshot",
        existing_type=JSONB,
        nullable=False,
        server_default=None,
    )

    op.drop_index("ix_allocation_entries_channel", table_name="test_run_allocation_entries")
    op.drop_table("test_run_allocation_entries")
    op.drop_table("test_run_allocations")

    op.drop_index("ix_test_run_sequences_sequence", table_name="test_run_sequences")
    op.drop_index("ix_test_run_sequences_test_run", table_name="test_run_sequences")
    op.drop_table("test_run_sequences")

    op.drop_constraint("ck_test_runs_mode_snapshot", "test_runs", type_="check")
    op.drop_column("test_runs", "mode")

    null_snapshots = bind.execute(
        sa.text("SELECT COUNT(*) FROM test_runs WHERE signal_snapshot_id IS NULL")
    ).scalar_one()
    if null_snapshots:
        raise RuntimeError(
            "Cannot downgrade: channel-mode test runs without signal snapshots exist",
        )

    op.alter_column(
        "test_runs",
        "signal_snapshot_id",
        existing_type=BIGINT_PK,
        nullable=False,
    )

    test_run_mode_enum.drop(bind, checkfirst=True)
