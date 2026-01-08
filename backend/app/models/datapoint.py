from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from sqlalchemy import DateTime, ForeignKey, Index, String, Text
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing helpers only
    from app.models.allocation import Allocation
    from app.models.project import Project


class Datapoint(Base):
    __tablename__ = "datapoints"
    __table_args__ = (Index("ix_dp_project_type", "project_id", "datapoint_type"),)

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    project_id: Mapped[int] = mapped_column(
        ForeignKey("projects.id", ondelete="CASCADE"), nullable=False, index=True
    )
    datapoint_code: Mapped[str] = mapped_column(String(255), nullable=False, unique=True, index=True)
    datapoint_type: Mapped[str] = mapped_column(String(50), nullable=False)
    name: Mapped[str] = mapped_column(String(100), nullable=False)

    voltage_level: Mapped[str | None] = mapped_column(String(50), nullable=True)
    bay: Mapped[str | None] = mapped_column(String(100), nullable=True)
    ied_name: Mapped[str | None] = mapped_column(String(100), nullable=True)
    hmi_text: Mapped[str | None] = mapped_column(Text, nullable=True)
    terminal: Mapped[str | None] = mapped_column(String(100), nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    project: Mapped["Project"] = relationship(
        "Project", back_populates="datapoints", lazy="selectin"
    )
    allocations: Mapped[list["Allocation"]] = relationship(
        "Allocation", back_populates="datapoint", cascade="all, delete-orphan", lazy="selectin"
    )

    @property
    def allocation(self) -> Allocation | None:  # pragma: no cover - convenience accessor
        if not self.allocations:
            return None
        return self.allocations[0]
