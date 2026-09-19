from __future__ import annotations

from datetime import datetime

from sqlalchemy import DateTime, Index, String, func
from sqlalchemy.orm import Mapped, mapped_column

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK


class CoreDiagnosticsAcknowledgement(Base):
    __tablename__: str = "core_diagnostics_acknowledgements"
    __table_args__: tuple[object, ...] = (
        Index("ix_core_diag_ack_incident", "hostname", "incident_id", "acknowledged_at"),
        Index("ix_core_diag_ack_time", "acknowledged_at"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    hostname: Mapped[str] = mapped_column(String(255), nullable=False)
    incident_id: Mapped[str] = mapped_column(String(128), nullable=False)
    actor: Mapped[str] = mapped_column(String(128), nullable=False)
    acknowledged_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
