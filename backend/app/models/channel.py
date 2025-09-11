from datetime import datetime, timezone
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import String, Integer, DateTime, ForeignKey
from app.infrastructure.db.database import Base


class Channel(Base):
    __tablename__ = "channels"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)

    # связь с устройством
    device_id: Mapped[int] = mapped_column(
        Integer, ForeignKey("devices.id", ondelete="CASCADE"), nullable=False
    )

    index: Mapped[int] = mapped_column(Integer, nullable=False)   # порядковый номер внутри устройства
    type: Mapped[str] = mapped_column(String, nullable=False)     # DO, DI, AO, AI и т.п.

    name: Mapped[str | None] = mapped_column(String, nullable=True)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc)
    )

    # ORM связь: один девайс → много каналов
    device = relationship("Device", back_populates="channels")
