"""add durable hardware command intents

Revision ID: d3e4f5a6b7c8
Revises: c2d3e4f5a6b7
"""

from alembic import op
import sqlalchemy as sa


revision = "d3e4f5a6b7c8"
down_revision = "c2d3e4f5a6b7"
branch_labels = None
depends_on = None
BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    op.create_table(
        "hardware_command_intents",
        sa.Column("command_id", sa.String(64), primary_key=True),
        sa.Column("workspace_id", BIGINT_PK, nullable=False),
        sa.Column("job_id", sa.String(64), nullable=True),
        sa.Column("attempt_id", sa.String(64), nullable=True),
        sa.Column("owner_kind", sa.String(16), nullable=False),
        sa.Column("owner_id", sa.String(64), nullable=False),
        sa.Column("device_id", BIGINT_PK, nullable=True),
        sa.Column("channel_id", BIGINT_PK, nullable=False),
        sa.Column("unit_id", sa.String(128), nullable=False),
        sa.Column("action", sa.String(32), nullable=False),
        sa.Column("payload", sa.JSON(), nullable=False, server_default=sa.text("'{}'")),
        sa.Column("status", sa.String(24), nullable=False, server_default="created"),
        sa.Column("execution_status", sa.String(24), nullable=False, server_default="unknown"),
        sa.Column("ack_packet_id", sa.Integer(), nullable=True),
        sa.Column("ack_status", sa.String(24), nullable=True),
        sa.Column("ack_error", sa.String(32), nullable=True),
        sa.Column("ack_received_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("fencing_epoch", sa.Integer(), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
    )
    op.create_index("ix_hardware_command_intents_workspace_created", "hardware_command_intents", ["workspace_id", "created_at"])
    op.create_index("ix_hardware_command_intents_job_created", "hardware_command_intents", ["job_id", "created_at"])
    op.create_index("ix_hardware_command_intents_channel_created", "hardware_command_intents", ["channel_id", "created_at"])


def downgrade() -> None:
    op.drop_index("ix_hardware_command_intents_channel_created", table_name="hardware_command_intents")
    op.drop_index("ix_hardware_command_intents_job_created", table_name="hardware_command_intents")
    op.drop_index("ix_hardware_command_intents_workspace_created", table_name="hardware_command_intents")
    op.drop_table("hardware_command_intents")
