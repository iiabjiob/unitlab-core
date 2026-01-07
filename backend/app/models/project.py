from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING
from uuid import uuid4

from sqlalchemy import DateTime, String
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing only
    from app.models.sequence import Sequence
    from app.models.switchgear import Switchgear
    from app.models.test_run import TestRun


class Project(Base):
    __tablename__ = "projects"

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    uuid: Mapped[str] = mapped_column(String(36), unique=True, nullable=False, default=lambda: str(uuid4()))
    name: Mapped[str] = mapped_column(String, nullable=False, index=True)

    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
        onupdate=func.now(),
        nullable=False,
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    switchgears: Mapped[list["Switchgear"]] = relationship(
        "Switchgear",
        back_populates="project",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    sequences: Mapped[list["Sequence"]] = relationship(
        "Sequence",
        back_populates="project",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    test_runs: Mapped[list["TestRun"]] = relationship(
        "TestRun",
        back_populates="project",
        cascade="all, delete-orphan",
        lazy="selectin",
    )

    def __repr__(self) -> str:  # pragma: no cover - debug helper
        return f"Project(id={self.id!r}, name={self.name!r})"
