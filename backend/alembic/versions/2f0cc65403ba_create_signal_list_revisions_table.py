"""create signal_list_revisions table

Revision ID: 2f0cc65403ba
Revises: 91697bed822b
Create Date: 2025-06-14 12:06:58.784216

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = '2f0cc65403ba'
down_revision: Union[str, None] = '0a4b81b85a02'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    """Upgrade schema."""
    op.create_table(
    'signal_list_revisions',
    sa.Column('id', sa.Integer, primary_key=True, autoincrement=True),
    sa.Column('project_name', sa.String(length=100), nullable=False),  # если будет несколько проектов
    sa.Column('version', sa.String(length=50), nullable=False),        # например v1.4, v2.0 и т.п.
    sa.Column('uploaded_at', sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
    sa.Column('description', sa.String(length=500), nullable=True),
    sa.Column('is_active', sa.Boolean, nullable=False, default=False)  # какая версия активна сейчас
)


def downgrade() -> None:
    """Downgrade schema."""
    op.drop_table('signal_list_revisions')
