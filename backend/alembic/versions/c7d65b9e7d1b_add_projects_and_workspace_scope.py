"""Add projects and scope workspace entities

Revision ID: c7d65b9e7d1b
Revises: 9f1e2d3c4b5a
Create Date: 2025-12-15 10:00:00.000000

"""
from __future__ import annotations

from typing import Sequence, Union
from uuid import uuid4

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "c7d65b9e7d1b"
down_revision: Union[str, None] = "9f1e2d3c4b5a"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def upgrade() -> None:
    op.create_table(
        "projects",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("uuid", sa.String(length=36), nullable=False, unique=True),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column(
            "updated_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
    )
    op.create_index("ix_projects_name", "projects", ["name"], unique=False)

    op.add_column(
        "switchgears",
        sa.Column("project_id", BIGINT_PK, nullable=True),
    )
    op.create_index(
        "ix_switchgears_project_name",
        "switchgears",
        ["project_id", "name"],
        unique=False,
    )
    op.create_foreign_key(
        "fk_switchgears_project_id",
        "switchgears",
        "projects",
        ["project_id"],
        ["id"],
        ondelete="CASCADE",
    )

    op.add_column(
        "sequences",
        sa.Column("project_id", BIGINT_PK, nullable=True),
    )
    op.create_index(
        "ix_sequences_project_name",
        "sequences",
        ["project_id", "name"],
        unique=False,
    )
    op.create_foreign_key(
        "fk_sequences_project_id",
        "sequences",
        "projects",
        ["project_id"],
        ["id"],
        ondelete="CASCADE",
    )

    bind = op.get_bind()
    default_uuid = str(uuid4())
    bind.execute(
        sa.text("INSERT INTO projects (name, uuid) VALUES (:name, :uuid)"),
        {"name": "Default Project", "uuid": default_uuid},
    )
    result = bind.execute(
        sa.text("SELECT id FROM projects WHERE uuid = :uuid"),
        {"uuid": default_uuid},
    )
    default_project_id = result.scalar_one()

    bind.execute(
        sa.text("UPDATE switchgears SET project_id = :pid WHERE project_id IS NULL"),
        {"pid": default_project_id},
    )
    bind.execute(
        sa.text("UPDATE sequences SET project_id = :pid WHERE project_id IS NULL"),
        {"pid": default_project_id},
    )

    op.alter_column("switchgears", "project_id", nullable=False)
    op.alter_column("sequences", "project_id", nullable=False)


def downgrade() -> None:
    op.alter_column("sequences", "project_id", nullable=True)
    op.drop_constraint("fk_sequences_project_id", "sequences", type_="foreignkey")
    op.drop_index("ix_sequences_project_name", table_name="sequences")
    op.drop_column("sequences", "project_id")

    op.alter_column("switchgears", "project_id", nullable=True)
    op.drop_constraint("fk_switchgears_project_id", "switchgears", type_="foreignkey")
    op.drop_index("ix_switchgears_project_name", table_name="switchgears")
    op.drop_column("switchgears", "project_id")

    op.drop_index("ix_projects_name", table_name="projects")
    op.drop_table("projects")
