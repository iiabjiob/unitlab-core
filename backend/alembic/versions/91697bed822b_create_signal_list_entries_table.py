"""create signal_list_entries table

Revision ID: 91697bed822b
Revises: 0a4b81b85a02
Create Date: 2025-06-14 11:58:41.783672

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = '91697bed822b'
down_revision: Union[str, None] = '2f0cc65403ba'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    """Upgrade schema."""
    op.create_table(
        'signal_list_entries',
        sa.Column('id', sa.Integer, primary_key=True, autoincrement=True),
        sa.Column('revision_id', sa.Integer, sa.ForeignKey('signal_list_revisions.id', ondelete='CASCADE'), nullable=False),
        sa.Column('unit_id', sa.String(length=100), nullable=True),
        sa.Column('channel_index', sa.Integer, nullable=True),
        sa.Column('terminal', sa.String(length=100), nullable=True),
        sa.Column('bay_name', sa.String(length=100), nullable=True),
        sa.Column('signal_name', sa.String(length=250), nullable=True),
        sa.Column('hmi_presentation_text', sa.String(length=500), nullable=True),
        sa.Column('signal_type', sa.String(length=10), nullable=True),
        sa.Column('group', sa.String(length=100), nullable=True),
        sa.Column('reaction_matrix', sa.String(length=100), nullable=True),
        sa.Column('external_address', sa.Integer, nullable=True),
        sa.Column('test_result', sa.String(length=10), nullable=True),
        sa.Column('tested_at', sa.DateTime(timezone=True), nullable=True),
    )


def downgrade() -> None:
    """Downgrade schema."""
    op.drop_table('signal_list_entries')
