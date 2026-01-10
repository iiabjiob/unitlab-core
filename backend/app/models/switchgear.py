# app/models/switchgear.py
from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from sqlalchemy import BigInteger, DateTime, ForeignKey, Integer, String
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - typing only
    from app.models.workspace import Workspace, WorkspaceSwitchgear


class Switchgear(Base):
    __tablename__ = "switchgears"
    __table_args__: tuple = ()

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
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
    workspace_links: Mapped[list["WorkspaceSwitchgear"]] = relationship(
        "WorkspaceSwitchgear",
        back_populates="switchgear",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    workspaces: Mapped[list["Workspace"]] = relationship(
        "Workspace",
        secondary="workspace_switchgears",
        viewonly=True,
        lazy="selectin",
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
