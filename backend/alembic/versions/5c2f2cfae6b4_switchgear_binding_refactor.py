"""switchgear binding refactor

Revision ID: 5c2f2cfae6b4
Revises: b3a1f4f64c0e
Create Date: 2025-12-12 10:00:00.000000

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "5c2f2cfae6b4"
down_revision: Union[str, None] = "b3a1f4f64c0e"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.alter_column("switchgears", "kind", new_column_name="switchgear_type")

    for column in ("do_open", "do_closed", "di_open", "di_close"):
        op.drop_column("switchgears", column)

    op.create_table(
        "switchgear_channel_bindings",
        sa.Column(
            "id",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            autoincrement=True,
            nullable=False,
        ),
        sa.Column(
            "switchgear_id",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            nullable=False,
        ),
        sa.Column(
            "channel_id",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            nullable=True,
        ),
        sa.Column("role", sa.String(length=32), nullable=False),
        sa.Column("delay_ms", sa.Integer(), server_default=sa.text("0"), nullable=False),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.ForeignKeyConstraint(["switchgear_id"], ["switchgears.id"], ondelete="CASCADE"),
        sa.ForeignKeyConstraint(["channel_id"], ["channels.id"], ondelete="SET NULL"),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("switchgear_id", "role", name="uq_switchgear_binding_role"),
    )
    op.create_index(
        "ix_switchgear_channel_bindings_switchgear_id",
        "switchgear_channel_bindings",
        ["switchgear_id"],
        unique=False,
    )
    op.create_index(
        "ix_switchgear_channel_bindings_channel_id",
        "switchgear_channel_bindings",
        ["channel_id"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index(
        "ix_switchgear_channel_bindings_channel_id",
        table_name="switchgear_channel_bindings",
    )
    op.drop_index(
        "ix_switchgear_channel_bindings_switchgear_id",
        table_name="switchgear_channel_bindings",
    )
    op.drop_table("switchgear_channel_bindings")

    op.alter_column("switchgears", "switchgear_type", new_column_name="kind")

    op.add_column(
        "switchgears",
        sa.Column(
            "do_open",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            nullable=True,
        ),
    )
    op.add_column(
        "switchgears",
        sa.Column(
            "do_closed",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            nullable=True,
        ),
    )
    op.add_column(
        "switchgears",
        sa.Column(
            "di_open",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            nullable=True,
        ),
    )
    op.add_column(
        "switchgears",
        sa.Column(
            "di_close",
            sa.BigInteger().with_variant(sa.Integer(), "sqlite"),
            nullable=True,
        ),
    )

    op.create_foreign_key(
        "fk_switchgears_do_open_channels",
        "switchgears",
        "channels",
        ["do_open"],
        ["id"],
        ondelete="SET NULL",
    )
    op.create_foreign_key(
        "fk_switchgears_do_closed_channels",
        "switchgears",
        "channels",
        ["do_closed"],
        ["id"],
        ondelete="SET NULL",
    )
    op.create_foreign_key(
        "fk_switchgears_di_open_channels",
        "switchgears",
        "channels",
        ["di_open"],
        ["id"],
        ondelete="SET NULL",
    )
    op.create_foreign_key(
        "fk_switchgears_di_close_channels",
        "switchgears",
        "channels",
        ["di_close"],
        ["id"],
        ondelete="SET NULL",
    )