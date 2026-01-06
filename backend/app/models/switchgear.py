# app/models/switchgear.py
from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from sqlalchemy import BigInteger, DateTime, ForeignKey, Index, Integer, String
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing only
    from app.models.project import Project


class Switchgear(Base):
    __tablename__ = "switchgears"
    __table_args__ = (Index("ix_switchgears_project_name", "project_id", "name"),)

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    project_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("projects.id", ondelete="CASCADE"), nullable=False
    )
    name: Mapped[str] = mapped_column(String, nullable=False)
    switchgear_type: Mapped[str] = mapped_column(String, nullable=False, default="switchgear")

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    bindings: Mapped[list["SwitchgearChannelBinding"]] = relationship(
        "SwitchgearChannelBinding",
        back_populates="switchgear",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    project: Mapped["Project"] = relationship(
        "Project", back_populates="switchgears", lazy="selectin"
    )


class SwitchgearChannelBinding(Base):
    __tablename__ = "switchgear_channel_bindings"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)

    switchgear_id: Mapped[int] = mapped_column(
        ForeignKey("switchgears.id", ondelete="CASCADE"), nullable=False
    )
    channel_id: Mapped[int | None] = mapped_column(
        ForeignKey("channels.id", ondelete="SET NULL"), nullable=True
    )

    role: Mapped[str] = mapped_column(String, nullable=False)
    delay_ms: Mapped[int] = mapped_column(Integer, default=0)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    switchgear = relationship("Switchgear", back_populates="bindings")
    channel = relationship("Channel", back_populates="switchgear_bindings")
