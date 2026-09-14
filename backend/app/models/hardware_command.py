from __future__ import annotations

from datetime import datetime
from typing import Any

from sqlalchemy import DateTime, Index, Integer, JSON, String, func
from sqlalchemy.orm import Mapped, mapped_column

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK


class HardwareCommandIntent(Base):
    __tablename__ = "hardware_command_intents"
    __table_args__ = (
        Index("ix_hardware_command_intents_workspace_created", "workspace_id", "created_at"),
        Index("ix_hardware_command_intents_job_created", "job_id", "created_at"),
        Index("ix_hardware_command_intents_channel_created", "channel_id", "created_at"),
    )

    command_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    workspace_id: Mapped[int] = mapped_column(BIGINT_PK, nullable=False)
    job_id: Mapped[str | None] = mapped_column(String(64), nullable=True)
    attempt_id: Mapped[str | None] = mapped_column(String(64), nullable=True)
    owner_kind: Mapped[str] = mapped_column(String(16), nullable=False)
    owner_id: Mapped[str] = mapped_column(String(64), nullable=False)
    device_id: Mapped[int | None] = mapped_column(BIGINT_PK, nullable=True)
    channel_id: Mapped[int] = mapped_column(BIGINT_PK, nullable=False)
    unit_id: Mapped[str] = mapped_column(String(128), nullable=False)
    action: Mapped[str] = mapped_column(String(32), nullable=False)
    payload: Mapped[dict[str, Any]] = mapped_column(JSON, nullable=False, server_default="{}")
    status: Mapped[str] = mapped_column(String(24), nullable=False, server_default="created")
    execution_status: Mapped[str] = mapped_column(String(24), nullable=False, server_default="unknown")
    ack_packet_id: Mapped[int | None] = mapped_column(Integer, nullable=True)
    ack_status: Mapped[str | None] = mapped_column(String(24), nullable=True)
    ack_error: Mapped[str | None] = mapped_column(String(32), nullable=True)
    ack_received_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    fencing_epoch: Mapped[int | None] = mapped_column(Integer, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now(), nullable=False)
