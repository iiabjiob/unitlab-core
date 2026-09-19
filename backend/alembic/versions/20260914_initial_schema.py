"""Create the clean UnitLab schema from the current domain model.

This is the first migration after the intentional database reset. Existing
databases are not upgraded through the retired migration chain; deployments
must recreate the disposable database and run ``alembic upgrade head``.
"""

from alembic import op

import app.models  # noqa: F401  # pyright: ignore[reportUnusedImport]  # imported for model registration side effects
from app.infrastructure.db.database import Base


revision = "20260914_initial_schema"
down_revision = None
branch_labels = None
depends_on = None


def upgrade() -> None:
    Base.metadata.create_all(bind=op.get_bind(), checkfirst=False)


def downgrade() -> None:
    Base.metadata.drop_all(bind=op.get_bind(), checkfirst=False)
