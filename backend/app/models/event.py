from __future__ import annotations

from datetime import datetime
from typing import Optional

from sqlalchemy import (
    BigInteger,
    CheckConstraint,
    DateTime,
    ForeignKey,
    Index,
    String,
    Text,
    JSON,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK


EVENT_TYPE_VALUES = ("cmd", "state", "system", "sequence", "status")
EVENT_DIRECTION_VALUES = ("in", "out")
EVENT_RESULT_VALUES = ("ok", "error", "timeout", "pending")


class Event(Base):
    __tablename__ = "events"

    __table_args__ = (
        Index("ix_events_ts", "ts"),
        Index("ix_events_project_ts", "project_id", "ts"),
        CheckConstraint(
            "event_type IN ('cmd','state','system','sequence','status')",
            name="ck_events_type",
        ),
        CheckConstraint(
            "direction IN ('in','out') OR direction IS NULL",
            name="ck_events_direction",
        ),
        CheckConstraint(
            "result IN ('ok','error','timeout','pending') OR result IS NULL",
            name="ck_events_result",
        ),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    project_id: Mapped[Optional[int]] = mapped_column(BigInteger, nullable=True)
    ts: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now(), nullable=False)

    event_type: Mapped[str] = mapped_column(String(16), nullable=False)

    datapoint_id: Mapped[Optional[int]] = mapped_column(BigInteger, nullable=True)
    device_id: Mapped[Optional[int]] = mapped_column(
        ForeignKey("devices.id", ondelete="SET NULL"), nullable=True
    )
    channel_id: Mapped[Optional[int]] = mapped_column(
        ForeignKey("channels.id", ondelete="SET NULL"), nullable=True
    )

    packet_id: Mapped[Optional[str]] = mapped_column(String(64), nullable=True)

    source: Mapped[str] = mapped_column(String(64), server_default="system", nullable=False)
    direction: Mapped[Optional[str]] = mapped_column(String(16), nullable=True)
    result: Mapped[Optional[str]] = mapped_column(String(16), nullable=True)

    payload: Mapped[Optional[dict]] = mapped_column(JSON, nullable=True)
    message: Mapped[Optional[str]] = mapped_column(Text, nullable=True)

    device = relationship("Device", lazy="selectin")
    channel = relationship("Channel", lazy="selectin")


__all__ = ["Event"]
