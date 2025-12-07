"""switchgear drop feedback column and rename title

Revision ID: 4a3c9d58a1f9
Revises: 5c2f2cfae6b4
Create Date: 2025-12-12 12:00:00.000000

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "4a3c9d58a1f9"
down_revision: Union[str, None] = "5c2f2cfae6b4"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.alter_column("switchgears", "title", new_column_name="name")
    op.drop_column("switchgears", "feedback_delay_ms")


def downgrade() -> None:
    op.add_column(
        "switchgears",
        sa.Column("feedback_delay_ms", sa.Integer(), server_default=sa.text("0"), nullable=False),
    )
    op.alter_column("switchgears", "name", new_column_name="title")
    op.alter_column(
        "switchgears",
        "feedback_delay_ms",
        server_default=None,
    )