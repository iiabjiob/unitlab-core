"""add IEC 61850 runtime selection

Revision ID: f4b5c6d7e8f9
Revises: f3a4b5c6d7e8
Create Date: 2026-06-09 00:00:00.000000
"""

from alembic import op
import sqlalchemy as sa


revision = "f4b5c6d7e8f9"
down_revision = "f3a4b5c6d7e8"
branch_labels = None
depends_on = None


BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    op.create_table(
        "workspace_iec61850_runtime_selections",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("scl_import_id", BIGINT_PK, sa.ForeignKey("workspace_iec61850_scl_imports.id", ondelete="RESTRICT"), nullable=False),
        sa.Column("runtime_revision", sa.Integer(), nullable=False, server_default="1"),
        sa.Column("selected_by", sa.String(length=128), nullable=True),
        sa.Column("selection_reason", sa.String(length=255), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
        sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
    )
    op.create_unique_constraint(
        "uq_w_iec61850_rt_sel_workspace",
        "workspace_iec61850_runtime_selections",
        ["workspace_id"],
    )
    op.create_index(
        "ix_w_iec61850_rt_sel_import",
        "workspace_iec61850_runtime_selections",
        ["scl_import_id"],
        unique=False,
    )
    op.create_table(
        "workspace_iec61850_runtime_selection_events",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("scl_import_id", BIGINT_PK, sa.ForeignKey("workspace_iec61850_scl_imports.id", ondelete="RESTRICT"), nullable=False),
        sa.Column("runtime_revision", sa.Integer(), nullable=False),
        sa.Column("operation", sa.String(length=32), nullable=False),
        sa.Column("selected_by", sa.String(length=128), nullable=True),
        sa.Column("selection_reason", sa.String(length=255), nullable=True),
        sa.Column("payload", sa.JSON(), nullable=False, server_default=sa.text("'{}'")),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
    )
    op.create_index(
        "ix_w_iec61850_rt_sel_events_ws_created",
        "workspace_iec61850_runtime_selection_events",
        ["workspace_id", "created_at", "id"],
        unique=False,
    )
    op.create_index(
        "ix_w_iec61850_rt_sel_events_import",
        "workspace_iec61850_runtime_selection_events",
        ["scl_import_id"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index("ix_w_iec61850_rt_sel_events_import", table_name="workspace_iec61850_runtime_selection_events")
    op.drop_index("ix_w_iec61850_rt_sel_events_ws_created", table_name="workspace_iec61850_runtime_selection_events")
    op.drop_table("workspace_iec61850_runtime_selection_events")
    op.drop_index("ix_w_iec61850_rt_sel_import", table_name="workspace_iec61850_runtime_selections")
    op.drop_constraint("uq_w_iec61850_rt_sel_workspace", "workspace_iec61850_runtime_selections", type_="unique")
    op.drop_table("workspace_iec61850_runtime_selections")
