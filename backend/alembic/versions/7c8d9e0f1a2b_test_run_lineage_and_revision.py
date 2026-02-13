"""add test-run lineage and allocation revision

Revision ID: 7c8d9e0f1a2b
Revises: 6a1b2c3d4e5f
Create Date: 2026-02-13 11:20:00.000000
"""

from __future__ import annotations

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "7c8d9e0f1a2b"
down_revision: Union[str, None] = "6a1b2c3d4e5f"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    op.add_column("test_runs", sa.Column("source_test_run_id", BIGINT_PK, nullable=True))
    op.add_column(
        "test_runs",
        sa.Column("allocation_revision", sa.Integer(), nullable=False, server_default="1"),
    )
    op.create_foreign_key(
        "fk_test_runs_source_test_run",
        "test_runs",
        "test_runs",
        ["source_test_run_id"],
        ["id"],
        ondelete="SET NULL",
    )
    op.create_index("ix_test_runs_source", "test_runs", ["source_test_run_id"], unique=False)


def downgrade() -> None:
    op.drop_index("ix_test_runs_source", table_name="test_runs")
    op.drop_constraint("fk_test_runs_source_test_run", "test_runs", type_="foreignkey")
    op.drop_column("test_runs", "allocation_revision")
    op.drop_column("test_runs", "source_test_run_id")
