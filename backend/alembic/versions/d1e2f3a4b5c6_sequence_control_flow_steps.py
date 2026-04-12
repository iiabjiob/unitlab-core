"""add control-flow step types for sequences

Revision ID: d1e2f3a4b5c6
Revises: c9a8b7d6e5f4
Create Date: 2026-04-12 11:30:00.000000
"""

from __future__ import annotations

from typing import Sequence, Union

from alembic import context, op
import sqlalchemy as sa


revision: str = "d1e2f3a4b5c6"
down_revision: Union[str, None] = "c9a8b7d6e5f4"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def _add_enum_value(value: str) -> None:
    ctxt = context.get_context()
    with ctxt.autocommit_block():
        op.execute(
            sa.text(f"ALTER TYPE sequence_step_type_enum ADD VALUE IF NOT EXISTS '{value}'")
        )


def upgrade() -> None:
    _add_enum_value("CALL_SEQUENCE")
    _add_enum_value("REPEAT_SEQUENCE")


def downgrade() -> None:
    # PostgreSQL enums do not support removing values safely without a full type rebuild.
    return None
