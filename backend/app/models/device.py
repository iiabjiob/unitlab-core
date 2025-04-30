from sqlalchemy import Column, String, Boolean, DateTime
from datetime import datetime, timezone
from app.db.database import Base  # лучше использовать твой Base, а не создавать новый

class Device(Base):
    __tablename__ = "devices"

    unit_id = Column(String, primary_key=True, index=True)
    type = Column(String)
    is_active = Column(Boolean, default=True)
    created_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
