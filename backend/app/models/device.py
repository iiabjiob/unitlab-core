from datetime import datetime
from sqlalchemy.sql import func
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import String, Integer, BigInteger, Boolean, DateTime
from app.infrastructure.db.database import Base


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    unit_id: Mapped[str] = mapped_column(String, unique=True, index=True, nullable=False)

    type: Mapped[str | None] = mapped_column(String, nullable=True)
    num_channels: Mapped[int | None] = mapped_column(Integer, nullable=True)
    firmware_version: Mapped[str | None] = mapped_column(String, nullable=True)

    # новые user-friendly поля
    name: Mapped[str | None] = mapped_column(String, nullable=True)
    location: Mapped[str | None] = mapped_column(String, nullable=True)

    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    # связь с каналами
    channels = relationship(
        "Channel",
        back_populates="device",
        cascade="all, delete-orphan",
        passive_deletes=True,
    )
