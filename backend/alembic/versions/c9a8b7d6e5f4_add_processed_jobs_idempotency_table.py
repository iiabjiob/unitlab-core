"""add processed_jobs idempotency table

Revision ID: c9a8b7d6e5f4
Revises: b8e1d2c3f4a6
Create Date: 2026-02-22 23:59:00.000000
"""

from __future__ import annotations

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "c9a8b7d6e5f4"
down_revision: Union[str, None] = "b8e1d2c3f4a6"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)

    if not inspector.has_table("processed_jobs"):
        op.create_table(
            "processed_jobs",
            sa.Column("worker_name", sa.String(length=128), nullable=False),
            sa.Column("job_id", sa.String(length=128), nullable=False),
            sa.Column("stream_name", sa.String(length=255), nullable=False),
            sa.Column("entry_id", sa.String(length=255), nullable=True),
            sa.Column(
                "processed_at",
                sa.DateTime(timezone=True),
                server_default=sa.text("now()"),
                nullable=False,
            ),
            sa.PrimaryKeyConstraint("worker_name", "job_id", name="pk_processed_jobs"),
        )

    existing_indexes = {idx["name"] for idx in inspector.get_indexes("processed_jobs")}
    if "ix_processed_jobs_processed_at" not in existing_indexes:
        op.create_index(
            "ix_processed_jobs_processed_at",
            "processed_jobs",
            ["processed_at"],
            unique=False,
        )


def downgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    if not inspector.has_table("processed_jobs"):
        return

    existing_indexes = {idx["name"] for idx in inspector.get_indexes("processed_jobs")}
    if "ix_processed_jobs_processed_at" in existing_indexes:
        op.drop_index("ix_processed_jobs_processed_at", table_name="processed_jobs")
    op.drop_table("processed_jobs")
