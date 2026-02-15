"""introduce singleton signal sheet and live signal allocations

Revision ID: 9a1b2c3d4e6f
Revises: 7c8d9e0f1a2b
Create Date: 2026-02-13 21:05:00.000000
"""

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "9a1b2c3d4e6f"
down_revision: Union[str, None] = "7c8d9e0f1a2b"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def _has_table(inspector: sa.Inspector, table_name: str) -> bool:
    return table_name in inspector.get_table_names()


def _has_index(inspector: sa.Inspector, table_name: str, index_name: str) -> bool:
    return any(index.get("name") == index_name for index in inspector.get_indexes(table_name))


def upgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)

    if not _has_table(inspector, "signal_sheets"):
        op.create_table(
            "signal_sheets",
            sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
            sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
            sa.Column("source_filename", sa.String(length=255), nullable=True),
            sa.Column("source_hash", sa.String(length=64), nullable=True),
            sa.Column("rows_count", sa.Integer(), nullable=False, server_default="0"),
            sa.Column("schema_version", sa.Integer(), nullable=False, server_default="2"),
            sa.Column("data", sa.JSON(), nullable=False, server_default=sa.text("'{}'")),
            sa.Column("import_meta", sa.JSON(), nullable=True),
            sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
            sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
            sa.UniqueConstraint("workspace_id", name="uq_signal_sheets_workspace"),
        )

    inspector = sa.inspect(bind)
    if not _has_index(inspector, "signal_sheets", "ix_signal_sheets_workspace"):
        op.create_index("ix_signal_sheets_workspace", "signal_sheets", ["workspace_id"], unique=False)
    if not _has_index(inspector, "signal_sheets", "ix_signal_sheets_workspace_updated"):
        op.create_index(
            "ix_signal_sheets_workspace_updated",
            "signal_sheets",
            ["workspace_id", "updated_at"],
            unique=False,
        )

    inspector = sa.inspect(bind)
    if not _has_table(inspector, "signal_sheet_presets"):
        op.create_table(
            "signal_sheet_presets",
            sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
            sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
            sa.Column("name", sa.String(length=120), nullable=False),
            sa.Column("import_meta", sa.JSON(), nullable=False, server_default=sa.text("'{}'")),
            sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
            sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
            sa.UniqueConstraint("workspace_id", "name", name="uq_signal_sheet_presets_workspace_name"),
        )

    inspector = sa.inspect(bind)
    if not _has_index(inspector, "signal_sheet_presets", "ix_signal_sheet_presets_workspace"):
        op.create_index("ix_signal_sheet_presets_workspace", "signal_sheet_presets", ["workspace_id"], unique=False)

    inspector = sa.inspect(bind)
    if not _has_table(inspector, "signal_allocations"):
        op.create_table(
            "signal_allocations",
            sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True, nullable=False),
            sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
            sa.Column("signal_id", BIGINT_PK, sa.ForeignKey("signals.id", ondelete="CASCADE"), nullable=False),
            sa.Column("channel_id", BIGINT_PK, sa.ForeignKey("channels.id", ondelete="CASCADE"), nullable=False),
            sa.Column("allocation_meta", sa.JSON(), nullable=True),
            sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
            sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
            sa.UniqueConstraint("workspace_id", "signal_id", name="uq_signal_allocations_signal"),
            sa.UniqueConstraint("workspace_id", "channel_id", name="uq_signal_allocations_channel"),
        )

    inspector = sa.inspect(bind)
    if not _has_index(inspector, "signal_allocations", "ix_signal_allocations_workspace"):
        op.create_index("ix_signal_allocations_workspace", "signal_allocations", ["workspace_id"], unique=False)
    if not _has_index(inspector, "signal_allocations", "ix_signal_allocations_channel"):
        op.create_index("ix_signal_allocations_channel", "signal_allocations", ["channel_id"], unique=False)

    inspector = sa.inspect(bind)
    if _has_table(inspector, "signal_snapshot_allocations"):
        if _has_index(inspector, "signal_snapshot_allocations", "ix_signal_snapshot_allocations_workspace"):
            op.drop_index("ix_signal_snapshot_allocations_workspace", table_name="signal_snapshot_allocations")
        if _has_index(inspector, "signal_snapshot_allocations", "ix_snapshot_allocations_workspace"):
            op.drop_index("ix_snapshot_allocations_workspace", table_name="signal_snapshot_allocations")
        op.drop_table("signal_snapshot_allocations")

    inspector = sa.inspect(bind)
    if _has_table(inspector, "signal_snapshots"):
        if _has_index(inspector, "signal_snapshots", "ix_signal_snapshots_workspace_updated"):
            op.drop_index("ix_signal_snapshots_workspace_updated", table_name="signal_snapshots")
        if _has_index(inspector, "signal_snapshots", "ix_signal_snapshots_workspace"):
            op.drop_index("ix_signal_snapshots_workspace", table_name="signal_snapshots")
        op.drop_table("signal_snapshots")

    if bind.dialect.name == "postgresql":
        op.execute("DROP TYPE IF EXISTS signal_snapshot_status_enum")


def downgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)

    if _has_table(inspector, "signal_allocations"):
        if _has_index(inspector, "signal_allocations", "ix_signal_allocations_channel"):
            op.drop_index("ix_signal_allocations_channel", table_name="signal_allocations")
        if _has_index(inspector, "signal_allocations", "ix_signal_allocations_workspace"):
            op.drop_index("ix_signal_allocations_workspace", table_name="signal_allocations")
        op.drop_table("signal_allocations")

    inspector = sa.inspect(bind)
    if _has_table(inspector, "signal_sheet_presets"):
        if _has_index(inspector, "signal_sheet_presets", "ix_signal_sheet_presets_workspace"):
            op.drop_index("ix_signal_sheet_presets_workspace", table_name="signal_sheet_presets")
        op.drop_table("signal_sheet_presets")

    inspector = sa.inspect(bind)
    if _has_table(inspector, "signal_sheets"):
        if _has_index(inspector, "signal_sheets", "ix_signal_sheets_workspace_updated"):
            op.drop_index("ix_signal_sheets_workspace_updated", table_name="signal_sheets")
        if _has_index(inspector, "signal_sheets", "ix_signal_sheets_workspace"):
            op.drop_index("ix_signal_sheets_workspace", table_name="signal_sheets")
        op.drop_table("signal_sheets")
