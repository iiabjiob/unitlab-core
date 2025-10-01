from datetime import datetime
from sqlalchemy.sql import func
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import String, Integer, BigInteger, DateTime, ForeignKey
from app.infrastructure.db.database import Base
from app.models.sequence_step import SequenceStep

class Channel(Base):
    __tablename__ = "channels"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)

    # связь с устройством
    device_id: Mapped[int] = mapped_column(
        Integer, ForeignKey("devices.id", ondelete="CASCADE"), nullable=False
    )

    index: Mapped[int] = mapped_column(Integer, nullable=False)   # порядковый номер внутри устройства
    type: Mapped[str] = mapped_column(String, nullable=False)     # DO, DI, AO, AI и т.п.

    name: Mapped[str | None] = mapped_column(String, nullable=True)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    # ORM связь: один девайс → много каналов
    device = relationship("Device", back_populates="channels")

    steps: Mapped[list["SequenceStep"]] = relationship(
        "SequenceStep", back_populates="channel"
    )
