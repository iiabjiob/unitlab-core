"""event log refactor

Revision ID: b3a1f4f64c0e
Revises: 7b0c1d1a89f0
Create Date: 2025-12-05 10:00:00.000000

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "b3a1f4f64c0e"
down_revision: Union[str, None] = "7b0c1d1a89f0"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.execute(sa.text("DROP TABLE IF EXISTS event_logs"))

    op.create_table(
        "events",
        sa.Column("id", sa.BigInteger().with_variant(sa.Integer(), "sqlite"), autoincrement=True, nullable=False),
        sa.Column("project_id", sa.BigInteger(), nullable=True),
        sa.Column("ts", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("event_type", sa.String(length=16), nullable=False),
        sa.Column("datapoint_id", sa.BigInteger(), nullable=True),
        sa.Column("device_id", sa.BigInteger().with_variant(sa.Integer(), "sqlite"), nullable=True),
        sa.Column("channel_id", sa.BigInteger().with_variant(sa.Integer(), "sqlite"), nullable=True),
        sa.Column("packet_id", sa.String(length=64), nullable=True),
        sa.Column("source", sa.String(length=64), server_default=sa.text("'system'"), nullable=False),
        sa.Column("direction", sa.String(length=16), nullable=True),
        sa.Column("result", sa.String(length=16), nullable=True),
        sa.Column("payload", sa.JSON(), nullable=True),
        sa.Column("message", sa.Text(), nullable=True),
        sa.ForeignKeyConstraint(["device_id"], ["devices.id"], ondelete="SET NULL"),
        sa.ForeignKeyConstraint(["channel_id"], ["channels.id"], ondelete="SET NULL"),
        sa.PrimaryKeyConstraint("id"),
        sa.CheckConstraint(
            "event_type IN ('cmd','state','system','sequence','status')",
            name="ck_events_type",
        ),
        sa.CheckConstraint(
            "direction IN ('in','out') OR direction IS NULL",
            name="ck_events_direction",
        ),
        sa.CheckConstraint(
            "result IN ('ok','error','timeout','pending') OR result IS NULL",
            name="ck_events_result",
        ),
    )
    op.create_index("ix_events_ts", "events", ["ts"], unique=False)
    op.create_index("ix_events_project_ts", "events", ["project_id", "ts"], unique=False)


def downgrade() -> None:
    op.drop_index("ix_events_project_ts", table_name="events")
    op.drop_index("ix_events_ts", table_name="events")
    op.drop_table("events")

    op.create_table(
        "event_logs",
        sa.Column("id", sa.BigInteger().with_variant(sa.Integer(), "sqlite"), autoincrement=True, nullable=False),
        sa.Column("ts", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("type", sa.String(), nullable=False),
        sa.Column("summary", sa.String(), nullable=True),
        sa.Column("channel_id", sa.BigInteger(), nullable=True),
        sa.Column("payload", sa.JSON(), nullable=True),
        sa.ForeignKeyConstraint(["channel_id"], ["channels.id"], ondelete="SET NULL"),
        sa.PrimaryKeyConstraint("id"),
    )
