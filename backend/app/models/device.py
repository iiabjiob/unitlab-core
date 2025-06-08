from sqlalchemy import Column, Integer, String, Boolean, DateTime
from datetime import datetime, timezone
from app.db.database import Base  # лучше использовать твой Base, а не создавать новый

class Device(Base):
    __tablename__ = "devices"

    id                  = Column(Integer, primary_key=True, autoincrement=True)
    unit_id             = Column(String, unique=True, index=True, nullable=False)
    type                = Column(String)
    channels            = Column(Integer, nullable=True)
    location            = Column(String, nullable=True)
    firmware_version    = Column(String, nullable=True)
    is_active           = Column(Boolean, default=True)
    created_at          = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
