"""add signal snapshot allocations table

Revision ID: 1a3b5c7d9e00
Revises: fe12ac34e5b7
Create Date: 2026-01-12 23:30:00.000000

"""
from __future__ import annotations

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import JSONB


revision: str = "1a3b5c7d9e00"
down_revision: Union[str, None] = "fe12ac34e5b7"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
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
        sa.Column(
            "mapping",
            JSONB,
            nullable=False,
            server_default=sa.text("'[]'::jsonb"),
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
        sa.UniqueConstraint(
            "signal_snapshot_id",
            name="uq_snapshot_allocation_signal_snapshot",
        ),
    )
    op.create_index(
        "ix_snapshot_allocations_workspace",
        "signal_snapshot_allocations",
        ["workspace_id"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index(
        "ix_snapshot_allocations_workspace",
        table_name="signal_snapshot_allocations",
    )
    op.drop_table("signal_snapshot_allocations")
