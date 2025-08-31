from sqlalchemy import Column, String, BigInteger, JSON, DateTime
from sqlalchemy.sql import func
from app.infrastructure.db.database import Base

class EventLog(Base):
    __tablename__ = "event_log"

    id = Column(String, primary_key=True, index=True)  # UUID или ULID
    ts = Column(BigInteger, nullable=False)            # ms timestamp
    dir = Column(String, nullable=False)               # "IN" | "OUT"
    source = Column(String, nullable=False)            # "WS_DEVICE" | "WS_COMMAND"
    channel_or_action = Column(String, nullable=False)
    unit_id = Column(String, nullable=True)
    type = Column(String, nullable=True)
    summary = Column(String, nullable=False)
    payload = Column(JSON, nullable=True)
    created_at = Column(DateTime(timezone=True), server_default=func.now())
