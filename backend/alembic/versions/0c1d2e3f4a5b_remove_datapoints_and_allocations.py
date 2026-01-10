"""drop datapoints and allocations tables

Revision ID: 0c1d2e3f4a5b
Revises: d7e8f9a0b1c2
Create Date: 2026-01-10 12:00:00.000000
"""

from alembic import op
import sqlalchemy as sa


revision = "0c1d2e3f4a5b"
down_revision = "d7e8f9a0b1c2"
branch_labels = None
depends_on = None


def upgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)

    if inspector.has_table("allocations"):
        op.drop_index("ix_allocations_project_channel", table_name="allocations")
        op.drop_index("ix_allocations_project_datapoint", table_name="allocations")
        op.drop_table("allocations")

    if inspector.has_table("datapoints"):
        op.drop_index("ix_dp_project_type", table_name="datapoints")
        op.drop_index("ix_datapoints_project_id", table_name="datapoints")
        op.drop_index("ix_datapoints_datapoint_code", table_name="datapoints")
        op.drop_table("datapoints")


def downgrade() -> None:
    op.create_table(
        "datapoints",
        sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
        sa.Column("project_id", sa.BigInteger(), nullable=False),
        sa.Column("datapoint_code", sa.String(length=255), nullable=False),
        sa.Column("datapoint_type", sa.String(length=50), nullable=False),
        sa.Column("name", sa.String(length=100), nullable=False),
        sa.Column("voltage_level", sa.String(length=50)),
        sa.Column("bay", sa.String(length=100)),
        sa.Column("ied_name", sa.String(length=100)),
        sa.Column("hmi_text", sa.Text()),
        sa.Column("terminal", sa.String(length=100)),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.ForeignKeyConstraint(["project_id"], ["projects.id"], ondelete="CASCADE"),
    )
    op.create_index("ix_dp_project_type", "datapoints", ["project_id", "datapoint_type"])
    op.create_index("ix_datapoints_project_id", "datapoints", ["project_id"])
    op.create_index("ix_datapoints_datapoint_code", "datapoints", ["datapoint_code"], unique=True)

    op.create_table(
        "allocations",
        sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
        sa.Column("project_id", sa.BigInteger(), sa.ForeignKey("projects.id", ondelete="CASCADE"), nullable=False),
        sa.Column("datapoint_id", sa.BigInteger(), sa.ForeignKey("datapoints.id", ondelete="CASCADE"), nullable=False),
        sa.Column("channel_id", sa.BigInteger(), sa.ForeignKey("channels.id", ondelete="SET NULL"), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.UniqueConstraint("datapoint_id", name="uq_allocation_datapoint"),
        sa.UniqueConstraint("channel_id", name="uq_allocation_channel"),
    )
    op.create_index(
        "ix_allocations_project_datapoint",
        "allocations",
        ["project_id", "datapoint_id"],
    )
    op.create_index(
        "ix_allocations_project_channel",
        "allocations",
        ["project_id", "channel_id"],
    )
