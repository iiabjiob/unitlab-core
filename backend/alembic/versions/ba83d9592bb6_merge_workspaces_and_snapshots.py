"""merge workspaces and snapshots

Revision ID: ba83d9592bb6
Revises: 10a2b3c4d5e6, 1f2e3d4c5b6a
Create Date: 2026-01-10 20:08:36.316596

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = 'ba83d9592bb6'
down_revision: Union[str, None] = ('10a2b3c4d5e6', '1f2e3d4c5b6a')
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    """Upgrade schema."""
    pass


def downgrade() -> None:
    """Downgrade schema."""
    pass
