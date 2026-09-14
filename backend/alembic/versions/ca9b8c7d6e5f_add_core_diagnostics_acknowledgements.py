"""add append-only core diagnostics acknowledgement audit records

Revision ID: ca9b8c7d6e5f
Revises: c9a8b7d6e5f4
Create Date: 2026-09-14 15:30:00.000000
"""

from __future__ import annotations

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "ca9b8c7d6e5f"
down_revision: Union[str, None] = "c9a8b7d6e5f4"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    if inspector.has_table("core_diagnostics_acknowledgements"):
        return
    op.create_table(
        "core_diagnostics_acknowledgements",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("hostname", sa.String(length=255), nullable=False),
        sa.Column("incident_id", sa.String(length=128), nullable=False),
        sa.Column("actor", sa.String(length=128), nullable=False),
        sa.Column("acknowledged_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
    )
    op.create_index(
        "ix_core_diag_ack_incident",
        "core_diagnostics_acknowledgements",
        ["hostname", "incident_id", "acknowledged_at"],
    )
    op.create_index(
        "ix_core_diag_ack_time",
        "core_diagnostics_acknowledgements",
        ["acknowledged_at"],
    )


def downgrade() -> None:
    op.drop_index("ix_core_diag_ack_time", table_name="core_diagnostics_acknowledgements")
    op.drop_index("ix_core_diag_ack_incident", table_name="core_diagnostics_acknowledgements")
    op.drop_table("core_diagnostics_acknowledgements")
