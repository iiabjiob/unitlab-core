from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from sqlalchemy import DateTime, ForeignKey, Integer, String, UniqueConstraint, Index
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.sequence import SequenceStep
from app.models.types import BIGINT_PK

if TYPE_CHECKING:  # pragma: no cover - only needed for typing
    from app.models.switchgear import SwitchgearChannelBinding
    from app.models.test_run import TestRunStep


class Channel(Base):
    __tablename__ = "channels"

    __table_args__ = (
        UniqueConstraint("device_id", "channel_index", name="uq_channels_device_index"),
        Index("ix_channels_device_id", "device_id"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    device_id: Mapped[int] = mapped_column(
        ForeignKey("devices.id", ondelete="CASCADE"), nullable=False, index=True
    )
    channel_index: Mapped[int] = mapped_column(Integer, nullable=False)
    channel_type: Mapped[str] = mapped_column(String, nullable=False)
    name: Mapped[str | None] = mapped_column(String, nullable=True)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    device = relationship("Device", back_populates="channels", lazy="selectin")
    
    steps: Mapped[list["SequenceStep"]] = relationship(
        "SequenceStep", back_populates="channel", lazy="selectin"
    )

    test_steps: Mapped[list["TestRunStep"]] = relationship(
        "TestRunStep", back_populates="channel", lazy="selectin"
    )

    switchgear_bindings: Mapped[list["SwitchgearChannelBinding"]] = relationship(
        "SwitchgearChannelBinding", back_populates="channel", lazy="selectin"
    )

    @property
    def resolved_name(self) -> str:
        if self.name and self.name.strip():
            return self.name
        return f"CH{self.channel_index + 1}"
