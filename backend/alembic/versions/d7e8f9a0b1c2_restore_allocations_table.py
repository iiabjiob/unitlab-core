"""restore datapoints + allocations tables

Revision ID: d7e8f9a0b1c2
Revises: c1d2e3f4g5h6
Create Date: 2026-01-08 13:25:00.000000
"""

from alembic import context, op
import sqlalchemy as sa


revision = "d7e8f9a0b1c2"
down_revision = "c1d2e3f4g5h6"
branch_labels = None
depends_on = None


def upgrade() -> None:
    if context.is_offline_mode():
        op.execute(
            """
            CREATE TABLE IF NOT EXISTS datapoints (
                id BIGSERIAL PRIMARY KEY,
                project_id BIGINT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
                datapoint_code VARCHAR(255) NOT NULL,
                datapoint_type VARCHAR(50) NOT NULL,
                name VARCHAR(100) NOT NULL,
                voltage_level VARCHAR(50),
                bay VARCHAR(100),
                ied_name VARCHAR(100),
                hmi_text TEXT,
                terminal VARCHAR(100),
                created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
                updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
            )
            """
        )
        op.execute("CREATE INDEX IF NOT EXISTS ix_dp_project_type ON datapoints (project_id, datapoint_type)")
        op.execute("CREATE INDEX IF NOT EXISTS ix_datapoints_project_id ON datapoints (project_id)")
        op.execute("CREATE UNIQUE INDEX IF NOT EXISTS ix_datapoints_datapoint_code ON datapoints (datapoint_code)")
        op.execute(
            """
            CREATE TABLE IF NOT EXISTS allocations (
                id BIGSERIAL PRIMARY KEY,
                project_id BIGINT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
                datapoint_id BIGINT NOT NULL REFERENCES datapoints(id) ON DELETE CASCADE,
                channel_id BIGINT REFERENCES channels(id) ON DELETE SET NULL,
                created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
                updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
                CONSTRAINT uq_allocation_datapoint UNIQUE (datapoint_id),
                CONSTRAINT uq_allocation_channel UNIQUE (channel_id)
            )
            """
        )
        op.execute("CREATE INDEX IF NOT EXISTS ix_allocations_project_datapoint ON allocations (project_id, datapoint_id)")
        op.execute("CREATE INDEX IF NOT EXISTS ix_allocations_project_channel ON allocations (project_id, channel_id)")
        return

    bind = op.get_bind()
    inspector = sa.inspect(bind)

    if not inspector.has_table("datapoints"):
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
            sa.ForeignKeyConstraint(["project_id"], ["projects.id"], ondelete="CASCADE"),
        )
        op.create_index("ix_dp_project_type", "datapoints", ["project_id", "datapoint_type"])
        op.create_index("ix_datapoints_project_id", "datapoints", ["project_id"])
        op.create_index(
            "ix_datapoints_datapoint_code",
            "datapoints",
            ["datapoint_code"],
            unique=True,
        )
    else:
        op.execute(sa.text("ALTER TABLE datapoints DROP COLUMN IF EXISTS snag_ref"))

    inspector = sa.inspect(op.get_bind())
    if not inspector.has_table("allocations"):
        op.create_table(
            "allocations",
            sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
            sa.Column(
                "project_id",
                sa.BigInteger(),
                sa.ForeignKey("projects.id", ondelete="CASCADE"),
                nullable=False,
            ),
            sa.Column(
                "datapoint_id",
                sa.BigInteger(),
                sa.ForeignKey("datapoints.id", ondelete="CASCADE"),
                nullable=False,
            ),
            sa.Column(
                "channel_id",
                sa.BigInteger(),
                sa.ForeignKey("channels.id", ondelete="SET NULL"),
                nullable=True,
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


def downgrade() -> None:
    if context.is_offline_mode():
        op.execute("DROP TABLE IF EXISTS allocations")
        op.execute("DROP TABLE IF EXISTS datapoints")
        return

    bind = op.get_bind()
    inspector = sa.inspect(bind)

    if inspector.has_table("allocations"):
        op.drop_index("ix_allocations_project_channel", table_name="allocations")
        op.drop_index("ix_allocations_project_datapoint", table_name="allocations")
        op.drop_table("allocations")

    if inspector.has_table("datapoints"):
        op.execute(sa.text("ALTER TABLE datapoints ADD COLUMN IF NOT EXISTS snag_ref TEXT"))
