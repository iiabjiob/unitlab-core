"""Persist workspace SLD documents and their revisions."""

from alembic import op
import sqlalchemy as sa


revision = "20260919_sld_document"
down_revision = "20260915_seq_run_check"
branch_labels = None
depends_on = None


def upgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    if not inspector.has_table("workspace_sld_documents"):
        op.create_table(
            "workspace_sld_documents",
            sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
            sa.Column("workspace_id", sa.BigInteger(), nullable=False),
            sa.Column("document_schema", sa.String(length=64), server_default="unitlab.sld.v1", nullable=False),
            sa.Column("revision", sa.Integer(), server_default="0", nullable=False),
            sa.Column("document", sa.JSON(), server_default="{}", nullable=False),
            sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
            sa.Column("updated_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
            sa.ForeignKeyConstraint(["workspace_id"], ["workspaces.id"], ondelete="CASCADE"),
            sa.UniqueConstraint("workspace_id", name="uq_workspace_sld_documents_workspace"),
        )
    if not inspector.has_table("workspace_sld_document_revisions"):
        op.create_table(
            "workspace_sld_document_revisions",
            sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
            sa.Column("document_id", sa.BigInteger(), nullable=False),
            sa.Column("revision", sa.Integer(), nullable=False),
            sa.Column("document_schema", sa.String(length=64), nullable=False),
            sa.Column("document", sa.JSON(), nullable=False),
            sa.Column("change_kind", sa.String(length=32), server_default="edit", nullable=False),
            sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
            sa.ForeignKeyConstraint(["document_id"], ["workspace_sld_documents.id"], ondelete="CASCADE"),
            sa.UniqueConstraint("document_id", "revision", name="uq_workspace_sld_document_revisions_revision"),
        )
    if "ix_workspace_sld_document_revisions_document_created" not in {
        index["name"] for index in inspector.get_indexes("workspace_sld_document_revisions")
    }:
        op.create_index(
            "ix_workspace_sld_document_revisions_document_created",
            "workspace_sld_document_revisions",
            ["document_id", "created_at", "id"],
        )


def downgrade() -> None:
    op.drop_index("ix_workspace_sld_document_revisions_document_created", table_name="workspace_sld_document_revisions")
    op.drop_table("workspace_sld_document_revisions")
    op.drop_table("workspace_sld_documents")
