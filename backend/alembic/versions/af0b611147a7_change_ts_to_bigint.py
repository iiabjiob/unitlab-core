"""change ts to bigint

Revision ID: af0b611147a7
Revises: 93601dc597c5
Create Date: 2025-08-31 13:15:37.741913

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = 'af0b611147a7'
down_revision: Union[str, None] = '93601dc597c5'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.alter_column("event_log", "ts", type_=sa.BigInteger())


def downgrade() -> None:
    op.alter_column("event_log", "ts", type_=sa.Integer())
