from __future__ import annotations

from datetime import datetime

from sqlalchemy import DateTime, Index, PrimaryKeyConstraint, String
from sqlalchemy.orm import Mapped, mapped_column
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base


class ProcessedJob(Base):
    __tablename__ = "processed_jobs"

    __table_args__ = (
        PrimaryKeyConstraint("worker_name", "job_id", name="pk_processed_jobs"),
        Index("ix_processed_jobs_processed_at", "processed_at"),
    )

    worker_name: Mapped[str] = mapped_column(String(128), nullable=False)
    job_id: Mapped[str] = mapped_column(String(128), nullable=False)
    stream_name: Mapped[str] = mapped_column(String(255), nullable=False)
    entry_id: Mapped[str | None] = mapped_column(String(255), nullable=True)
    processed_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
        nullable=False,
    )

