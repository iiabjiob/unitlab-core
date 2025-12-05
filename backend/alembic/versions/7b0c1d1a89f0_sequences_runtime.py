"""Add sequence runtime tables and enums

Revision ID: 7b0c1d1a89f0
Revises: 55cb4ecb6cd2
Create Date: 2025-12-05 12:00:00.000000

"""
from __future__ import annotations

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import ENUM

revision: str = "7b0c1d1a89f0"
down_revision: str | None = "55cb4ecb6cd2"
branch_labels = None
depends_on = None


SEQUENCE_STEP_VALUES = ("WAIT", "DO_LATCH", "DO_PULSE", "DO_PAIR", "DO_BITMASK", "AO_SET")
SEQUENCE_RUN_STATUS_VALUES = ("running", "completed", "stopped", "error")
SEQUENCE_RUN_STEP_STATUS_VALUES = ("pending", "running", "completed", "error", "cancelled")

sequence_step_enum = ENUM(*SEQUENCE_STEP_VALUES, name="sequence_step_type_enum", create_type=True)
sequence_run_status_enum = ENUM(
    *SEQUENCE_RUN_STATUS_VALUES, name="sequence_run_status_enum", create_type=True
)
sequence_run_step_status_enum = ENUM(
    *SEQUENCE_RUN_STEP_STATUS_VALUES, name="sequence_run_step_status_enum", create_type=True
)

sequence_run_status_enum_no_create = ENUM(
    *SEQUENCE_RUN_STATUS_VALUES, name="sequence_run_status_enum", create_type=False
)
sequence_run_step_status_enum_no_create = ENUM(
    *SEQUENCE_RUN_STEP_STATUS_VALUES,
    name="sequence_run_step_status_enum",
    create_type=False,
)


def upgrade() -> None:
    bind = op.get_bind()
    sequence_step_enum.create(bind, checkfirst=True)
    sequence_run_status_enum.create(bind, checkfirst=True)
    sequence_run_step_status_enum.create(bind, checkfirst=True)

    with op.batch_alter_table("sequences") as batch:
        batch.add_column(
            sa.Column(
                "updated_at",
                sa.DateTime(timezone=True),
                server_default=sa.text("now()"),
                nullable=False,
            )
        )

    # rename column first so the data fix can target the new name without batch context limitations
    with op.batch_alter_table("sequence_steps") as batch:
        batch.alter_column("kind", new_column_name="sequence_step_type")

    # normalize legacy values so they match the enum labels exactly
    op.execute(
        "UPDATE sequence_steps SET sequence_step_type = upper(sequence_step_type)"
    )

    with op.batch_alter_table("sequence_steps") as batch:
        batch.alter_column(
            "sequence_step_type",
            existing_type=sa.String(),
            type_=sequence_step_enum,
            nullable=False,
            postgresql_using="sequence_step_type::sequence_step_type_enum",
        )
        batch.add_column(
            sa.Column(
                "created_at",
                sa.DateTime(timezone=True),
                server_default=sa.text("now()"),
                nullable=False,
            )
        )
        batch.add_column(
            sa.Column(
                "updated_at",
                sa.DateTime(timezone=True),
                server_default=sa.text("now()"),
                nullable=False,
            )
        )
        batch.create_unique_constraint("uq_sequence_steps_order", ["sequence_id", "order_index"])

    op.create_index(
        "ix_sequence_steps_seq_order",
        "sequence_steps",
        ["sequence_id", "order_index"],
    )

    op.create_table(
        "sequence_runs",
        sa.Column(
            "id",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            primary_key=True,
            autoincrement=True,
        ),
        sa.Column(
            "sequence_id",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            sa.ForeignKey("sequences.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column(
            "status",
            sequence_run_status_enum_no_create,
            nullable=False,
            server_default="running",
        ),
            sa.CheckConstraint(
                "status IN ('running','completed','stopped','error')",
                name="ck_sequence_runs_status",
            ),
        sa.Column(
            "started_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.Column("finished_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("error_message", sa.Text(), nullable=True),
        sa.Column("current_step_index", sa.Integer(), nullable=False, server_default="0"),
    )
    op.create_index(
        "ix_sequence_runs_sequence_started",
        "sequence_runs",
        ["sequence_id", "started_at"],
    )

    op.create_table(
        "sequence_run_steps",
        sa.Column(
            "id",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            primary_key=True,
            autoincrement=True,
        ),
        sa.Column(
            "run_id",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            sa.ForeignKey("sequence_runs.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column(
            "sequence_step_id",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            sa.ForeignKey("sequence_steps.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("order_index", sa.Integer(), nullable=False),
        sa.Column(
            "status",
            sequence_run_step_status_enum_no_create,
            nullable=False,
            server_default="pending",
        ),
        sa.Column("started_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("finished_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("error_message", sa.Text(), nullable=True),
        sa.Column("elapsed_ms", sa.Integer(), nullable=True),
            sa.CheckConstraint(
                "status IN ('pending','running','completed','error','cancelled')",
                name="ck_sequence_run_steps_status",
            ),
    )
    op.create_index(
        "ix_sequence_run_steps_run_order",
        "sequence_run_steps",
        ["run_id", "order_index"],
    )


def downgrade() -> None:
    op.drop_index("ix_sequence_run_steps_run_order", table_name="sequence_run_steps")
    op.drop_table("sequence_run_steps")
    op.drop_index("ix_sequence_runs_sequence_started", table_name="sequence_runs")
    op.drop_table("sequence_runs")

    op.drop_index("ix_sequence_steps_seq_order", table_name="sequence_steps")
    with op.batch_alter_table("sequence_steps") as batch:
        batch.drop_constraint("uq_sequence_steps_order", type_="unique")
        batch.drop_column("updated_at")
        batch.drop_column("created_at")
        batch.alter_column(
            "sequence_step_type",
            existing_type=sequence_step_enum,
            type_=sa.String(),
            nullable=False,
            postgresql_using="sequence_step_type::text",
        )
        batch.alter_column("sequence_step_type", new_column_name="kind")

    with op.batch_alter_table("sequences") as batch:
        batch.drop_column("updated_at")

    sequence_run_step_status_enum.drop(op.get_bind(), checkfirst=True)
    sequence_run_status_enum.drop(op.get_bind(), checkfirst=True)
    sequence_step_enum.drop(op.get_bind(), checkfirst=True)