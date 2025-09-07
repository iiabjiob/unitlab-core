from datetime import datetime
from sqlalchemy.orm import Mapped, mapped_column
from sqlalchemy import String, BigInteger, JSON, DateTime
from sqlalchemy.sql import func
from app.infrastructure.db.database import Base


class EventLog(Base):
    __tablename__ = "event_log"

    # UUID или ULID
    id: Mapped[str] = mapped_column(String, primary_key=True, index=True)

    # unix ms timestamp
    ts: Mapped[int] = mapped_column(BigInteger, nullable=False)

    # "IN" | "OUT"
    dir: Mapped[str] = mapped_column(String, nullable=False)

    # "WS_DEVICE" | "WS_COMMAND"
    source: Mapped[str] = mapped_column(String, nullable=False)

    channel_or_action: Mapped[str] = mapped_column(String, nullable=False)
    unit_id: Mapped[str | None] = mapped_column(String, nullable=True)
    type: Mapped[str | None] = mapped_column(String, nullable=True)
    summary: Mapped[str] = mapped_column(String, nullable=False)
    payload: Mapped[dict | None] = mapped_column(JSON, nullable=True)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now()
    )
