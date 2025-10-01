# app/models/switchgear.py
from datetime import datetime
from sqlalchemy.sql import func
from sqlalchemy.orm import Mapped, mapped_column
from sqlalchemy import String, Integer, DateTime, ForeignKey, BigInteger
from app.infrastructure.db.database import Base

class Switchgear(Base):
    __tablename__ = "switchgears"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    title: Mapped[str] = mapped_column(String, nullable=False)
    kind: Mapped[str] = mapped_column(String, nullable=False, default="switchgear")

    do_open: Mapped[int | None] = mapped_column(ForeignKey("channels.id", ondelete="SET NULL"))
    do_closed: Mapped[int | None] = mapped_column(ForeignKey("channels.id", ondelete="SET NULL"))
    di_open: Mapped[int | None] = mapped_column(ForeignKey("channels.id", ondelete="SET NULL"))
    di_close: Mapped[int | None] = mapped_column(ForeignKey("channels.id", ondelete="SET NULL"))

    feedback_delay_ms: Mapped[int] = mapped_column(Integer, default=0)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
