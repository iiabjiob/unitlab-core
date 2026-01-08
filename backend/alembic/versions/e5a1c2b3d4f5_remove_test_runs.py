"""Remove test run tables and enums

Revision ID: e5a1c2b3d4f5
Revises: d4f1e6a7b8c9
Create Date: 2026-01-08 00:00:00.000000

"""
from __future__ import annotations

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql


revision: str = "e5a1c2b3d4f5"
down_revision: Union[str, None] = "d4f1e6a7b8c9"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")
TEST_RUN_STATUS_VALUES = ("pending", "running", "completed", "failed", "cancelled")
test_run_status_enum = postgresql.ENUM(
    *TEST_RUN_STATUS_VALUES,
    name="test_run_status_enum",
)


def status_enum_column():
    return postgresql.ENUM(
        *TEST_RUN_STATUS_VALUES,
        name="test_run_status_enum",
        create_type=False,
    )


def upgrade() -> None:
    op.drop_index("ix_test_run_steps_run_order", table_name="test_run_steps")
    op.drop_table("test_run_steps")

    op.drop_index("ix_test_runs_project_created_at", table_name="test_runs")
    op.drop_table("test_runs")

    bind = op.get_bind()
    test_run_status_enum.drop(bind, checkfirst=True)


def downgrade() -> None:
    bind = op.get_bind()
    test_run_status_enum.create(bind, checkfirst=True)

    op.create_table(
        "test_runs",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "project_id",
            BIGINT_PK,
            sa.ForeignKey("projects.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("name", sa.String(length=120), nullable=False),
        sa.Column(
            "status",
            status_enum_column(),
            nullable=False,
            server_default="pending",
        ),
        sa.Column("settings", sa.JSON(), nullable=False, server_default=sa.text("'{}'")),
        sa.Column(
            "current_step_index",
            sa.Integer(),
            nullable=False,
            server_default="0",
        ),
        sa.Column("error_message", sa.Text(), nullable=True),
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
        sa.Column("started_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("finished_at", sa.DateTime(timezone=True), nullable=True),
    )
    op.create_index(
        "ix_test_runs_project_created_at",
        "test_runs",
        ["project_id", "created_at"],
        unique=False,
    )

    op.create_table(
        "test_run_steps",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column(
            "test_run_id",
            BIGINT_PK,
            sa.ForeignKey("test_runs.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("order_index", sa.Integer(), nullable=False),
        sa.Column(
            "channel_id",
            BIGINT_PK,
            sa.ForeignKey("channels.id", ondelete="SET NULL"),
            nullable=True,
        ),
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
        sa.UniqueConstraint("test_run_id", "order_index", name="uq_test_run_steps_order"),
    )
    op.create_index(
        "ix_test_run_steps_run_order",
        "test_run_steps",
        ["test_run_id", "order_index"],
        unique=False,
    )
