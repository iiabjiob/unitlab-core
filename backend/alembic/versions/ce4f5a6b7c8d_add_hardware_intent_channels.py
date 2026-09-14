"""persist every physical channel covered by a hardware intent"""

import json

import sqlalchemy as sa
from alembic import context, op


revision = "ce4f5a6b7c8d"
down_revision = "cd3f4a5b6c7d"
branch_labels = None
depends_on = None


def _table() -> sa.Table:
    return sa.table(
        "hardware_command_intent_channels",
        sa.column("command_id", sa.String(64)),
        sa.column("channel_id", sa.BigInteger()),
    )


def upgrade() -> None:
    op.create_table(
        "hardware_command_intent_channels",
        sa.Column(
            "command_id",
            sa.String(64),
            sa.ForeignKey("hardware_command_intents.command_id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("channel_id", sa.BigInteger(), nullable=False),
        sa.PrimaryKeyConstraint("command_id", "channel_id"),
        sa.UniqueConstraint(
            "command_id",
            "channel_id",
            name="uq_hardware_command_intent_channels_command_channel",
        ),
    )
    op.create_index(
        "ix_hardware_command_intent_channels_channel",
        "hardware_command_intent_channels",
        ["channel_id"],
        unique=False,
    )

    if context.is_offline_mode():
        return

    bind = op.get_bind()
    rows = bind.execute(
        sa.text("SELECT command_id, channel_id, payload FROM hardware_command_intents")
    ).mappings()
    insert = _table().insert()
    values: list[dict[str, int | str]] = []
    for row in rows:
        channel_ids = [int(row["channel_id"])]
        payload = row["payload"]
        if isinstance(payload, str):
            try:
                payload = json.loads(payload)
            except (TypeError, ValueError):
                payload = None
        raw_channel_ids = payload.get("channel_ids") if isinstance(payload, dict) else None
        if isinstance(raw_channel_ids, list):
            for raw_channel_id in raw_channel_ids:
                try:
                    normalized = int(raw_channel_id)
                except (TypeError, ValueError):
                    continue
                if normalized > 0 and normalized not in channel_ids:
                    channel_ids.append(normalized)
        values.extend(
            {"command_id": str(row["command_id"]), "channel_id": channel_id}
            for channel_id in channel_ids
        )
    if values:
        bind.execute(insert, values)


def downgrade() -> None:
    op.drop_index(
        "ix_hardware_command_intent_channels_channel",
        table_name="hardware_command_intent_channels",
    )
    op.drop_table("hardware_command_intent_channels")
