"""replace projects with workspaces and attachment tables

Revision ID: 1f2e3d4c5b6a
Revises: 0c1d2e3f4a5b
Create Date: 2026-01-20 12:00:00.000000
"""
from __future__ import annotations

from typing import Sequence, Union
import re
from uuid import uuid4

from alembic import op
import sqlalchemy as sa


revision: str = "1f2e3d4c5b6a"
down_revision: Union[str, None] = "0c1d2e3f4a5b"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

BIGINT_PK = sa.BigInteger().with_variant(sa.Integer(), "sqlite")


def _slugify(source: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", source.lower()).strip("-")
    return slug or "workspace"


def upgrade() -> None:
    op.create_table(
        "workspaces",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("uuid", sa.String(length=36), nullable=False, unique=True),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column("slug", sa.String(length=255), nullable=False, unique=True),
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
    op.create_index("ix_workspaces_name", "workspaces", ["name"], unique=False)

    op.create_table(
        "workspace_switchgears",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("switchgear_id", BIGINT_PK, sa.ForeignKey("switchgears.id", ondelete="CASCADE"), nullable=False),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.UniqueConstraint("workspace_id", "switchgear_id", name="uq_workspace_switchgear"),
    )
    op.create_index(
        "ix_workspace_switchgears_workspace",
        "workspace_switchgears",
        ["workspace_id"],
        unique=False,
    )
    op.create_index(
        "ix_workspace_switchgears_switchgear",
        "workspace_switchgears",
        ["switchgear_id"],
        unique=False,
    )

    op.create_table(
        "workspace_sequences",
        sa.Column("id", BIGINT_PK, primary_key=True, autoincrement=True),
        sa.Column("workspace_id", BIGINT_PK, sa.ForeignKey("workspaces.id", ondelete="CASCADE"), nullable=False),
        sa.Column("sequence_id", BIGINT_PK, sa.ForeignKey("sequences.id", ondelete="CASCADE"), nullable=False),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.UniqueConstraint("workspace_id", "sequence_id", name="uq_workspace_sequence"),
    )
    op.create_index(
        "ix_workspace_sequences_workspace",
        "workspace_sequences",
        ["workspace_id"],
        unique=False,
    )
    op.create_index(
        "ix_workspace_sequences_sequence",
        "workspace_sequences",
        ["sequence_id"],
        unique=False,
    )

    bind = op.get_bind()
    projects = bind.execute(
        sa.text(
            "SELECT id, uuid, name, created_at, updated_at FROM projects ORDER BY id"
        )
    ).mappings().all()

    used_slugs: set[str] = set()
    for project in projects:
        base_slug = _slugify(project["name"] or "workspace")
        slug = base_slug
        suffix = 2
        while slug in used_slugs:
            slug = f"{base_slug}-{suffix}"
            suffix += 1
        used_slugs.add(slug)
        bind.execute(
            sa.text(
                """
                INSERT INTO workspaces (id, uuid, name, slug, created_at, updated_at)
                VALUES (:id, :uuid, :name, :slug, :created_at, :updated_at)
                """
            ),
            {
                "id": project["id"],
                "uuid": project["uuid"] or str(uuid4()),
                "name": project["name"],
                "slug": slug,
                "created_at": project["created_at"],
                "updated_at": project["updated_at"],
            },
        )

    bind.execute(
        sa.text(
            """
            INSERT INTO workspace_switchgears (workspace_id, switchgear_id)
            SELECT project_id, id FROM switchgears
            """
        )
    )
    bind.execute(
        sa.text(
            """
            INSERT INTO workspace_sequences (workspace_id, sequence_id)
            SELECT project_id, id FROM sequences
            """
        )
    )

    op.drop_constraint("fk_switchgears_project_id", "switchgears", type_="foreignkey")
    op.drop_index("ix_switchgears_project_name", table_name="switchgears")
    op.drop_column("switchgears", "project_id")

    op.drop_constraint("fk_sequences_project_id", "sequences", type_="foreignkey")
    op.drop_index("ix_sequences_project_name", table_name="sequences")
    op.drop_column("sequences", "project_id")

    op.drop_index("ix_projects_name", table_name="projects")
    op.drop_table("projects")

    if bind.dialect.name == "postgresql":
        bind.execute(
            sa.text("SELECT setval('workspaces_id_seq', COALESCE((SELECT MAX(id) FROM workspaces), 1))")
        )


def downgrade() -> None:
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

    bind = op.get_bind()
    workspaces = bind.execute(
        sa.text(
            "SELECT id, uuid, name, created_at, updated_at FROM workspaces ORDER BY id"
        )
    ).mappings().all()
    for workspace in workspaces:
        bind.execute(
            sa.text(
                """
                INSERT INTO projects (id, uuid, name, created_at, updated_at)
                VALUES (:id, :uuid, :name, :created_at, :updated_at)
                """
            ),
            {
                "id": workspace["id"],
                "uuid": workspace["uuid"],
                "name": workspace["name"],
                "created_at": workspace["created_at"],
                "updated_at": workspace["updated_at"],
            },
        )

    if bind.dialect.name == "postgresql":
        bind.execute(
            sa.text("SELECT setval('projects_id_seq', COALESCE((SELECT MAX(id) FROM projects), 1))")
        )

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

    bind.execute(
        sa.text(
            """
            UPDATE switchgears AS s
            SET project_id = ws.workspace_id
            FROM (
                SELECT switchgear_id, MIN(workspace_id) AS workspace_id
                FROM workspace_switchgears
                GROUP BY switchgear_id
            ) AS ws
            WHERE s.id = ws.switchgear_id
            """
        )
    )
    bind.execute(
        sa.text(
            """
            UPDATE sequences AS seq
            SET project_id = ws.workspace_id
            FROM (
                SELECT sequence_id, MIN(workspace_id) AS workspace_id
                FROM workspace_sequences
                GROUP BY sequence_id
            ) AS ws
            WHERE seq.id = ws.sequence_id
            """
        )
    )

    op.alter_column("switchgears", "project_id", nullable=False)
    op.alter_column("sequences", "project_id", nullable=False)

    op.drop_index("ix_workspace_switchgears_switchgear", table_name="workspace_switchgears")
    op.drop_index("ix_workspace_switchgears_workspace", table_name="workspace_switchgears")
    op.drop_table("workspace_switchgears")

    op.drop_index("ix_workspace_sequences_sequence", table_name="workspace_sequences")
    op.drop_index("ix_workspace_sequences_workspace", table_name="workspace_sequences")
    op.drop_table("workspace_sequences")

    op.drop_index("ix_workspaces_name", table_name="workspaces")
    op.drop_table("workspaces")
