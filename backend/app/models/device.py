from __future__ import annotations

from datetime import datetime
from typing import Optional

from sqlalchemy import DateTime, Index, Integer, String
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK


class Device(Base):
    __tablename__ = "devices"

    __table_args__ = (
        Index("ix_devices_type_unit", "device_type", "unit_id"),
        Index("ix_devices_last_seen_at", "last_seen_at"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    unit_id: Mapped[str] = mapped_column(String, unique=True, nullable=False, index=True)
    device_type: Mapped[Optional[str]] = mapped_column(String, nullable=True)
    num_channels: Mapped[Optional[int]] = mapped_column(Integer, nullable=True)
    firmware_version: Mapped[Optional[str]] = mapped_column(String, nullable=True)
    name: Mapped[Optional[str]] = mapped_column(String, nullable=True)

    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    last_seen_at: Mapped[Optional[datetime]] = mapped_column(
        DateTime(timezone=True), nullable=True
    )
    registered_at: Mapped[Optional[datetime]] = mapped_column(
        DateTime(timezone=True), nullable=True
    )

    channels = relationship(
        "Channel",
        back_populates="device",
        cascade="all, delete-orphan",
        lazy="selectin",
    )

    @property
    def last_seen(self) -> Optional[int]:
        if self.last_seen_at is None:
            return None
        return int(self.last_seen_at.timestamp() * 1000)

    @property
    def registered_at_ms(self) -> Optional[int]:
        if self.registered_at is None:
            return None
        return int(self.registered_at.timestamp() * 1000)
