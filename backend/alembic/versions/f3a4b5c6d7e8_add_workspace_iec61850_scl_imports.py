"""add workspace IEC 61850 SCL imports

Revision ID: f3a4b5c6d7e8
Revises: f2a3b4c5d6e8
Create Date: 2026-06-09 00:00:00.000000
"""

from alembic import op
import sqlalchemy as sa


revision = "f3a4b5c6d7e8"
down_revision = "f2a3b4c5d6e8"
branch_labels = None
depends_on = None


BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    op.create_table(
        "workspace_iec61850_scl_imports",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("source_filename", sa.String(length=255), nullable=True),
        sa.Column("source_hash", sa.String(length=64), nullable=False),
        sa.Column("source_size", BIGINT_PK, nullable=False),
        sa.Column("selected_ied", sa.String(length=128), nullable=False),
        sa.Column("normalized_schema", sa.String(length=128), nullable=False),
        sa.Column("source_bytes", sa.LargeBinary(), nullable=False),
        sa.Column("normalized_model", sa.JSON(), nullable=False, server_default=sa.text("'{}'")),
        sa.Column("diagnostics", sa.JSON(), nullable=False, server_default=sa.text("'[]'")),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
        sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
    )
    op.create_unique_constraint(
        "uq_workspace_iec61850_scl_import_hash_ied",
        "workspace_iec61850_scl_imports",
        ["workspace_id", "source_hash", "selected_ied"],
    )
    op.create_index(
        "ix_workspace_iec61850_scl_imports_workspace_created",
        "workspace_iec61850_scl_imports",
        ["workspace_id", "created_at", "id"],
        unique=False,
    )
    op.create_index(
        "ix_workspace_iec61850_scl_imports_hash",
        "workspace_iec61850_scl_imports",
        ["source_hash"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index("ix_workspace_iec61850_scl_imports_hash", table_name="workspace_iec61850_scl_imports")
    op.drop_index("ix_workspace_iec61850_scl_imports_workspace_created", table_name="workspace_iec61850_scl_imports")
    op.drop_constraint("uq_workspace_iec61850_scl_import_hash_ied", "workspace_iec61850_scl_imports", type_="unique")
    op.drop_table("workspace_iec61850_scl_imports")
