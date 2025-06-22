from sqlalchemy import Column, Integer, String, Boolean, DateTime, ForeignKey
from sqlalchemy.orm import relationship
from app.db.database import Base
from datetime import datetime

class SignalListRevision(Base):
    __tablename__ = "signal_list_revisions"

    id = Column(Integer, primary_key=True, autoincrement=True)
    project_name = Column(String(length=100), nullable=False)
    version = Column(String(length=50), nullable=False)
    uploaded_at = Column(DateTime(timezone=True), nullable=False, default=datetime.utcnow)
    description = Column(String(length=500), nullable=True)
    is_active = Column(Boolean, nullable=False, default=False)

    # Для удобной навигации SQLAlchemy
    entries = relationship("SignalListEntry", back_populates="revision", cascade="all, delete-orphan")


class SignalListEntry(Base):
    __tablename__ = "signal_list_entries"

    id = Column(Integer, primary_key=True, autoincrement=True)
    revision_id = Column(Integer, ForeignKey("signal_list_revisions.id", ondelete="CASCADE"), nullable=False)

    unit_id = Column(String(length=100), nullable=True)
    channel_index = Column(Integer, nullable=True)
    terminal = Column(String(length=100), nullable=True)
    bay_name = Column(String(length=100), nullable=True)
    signal_name = Column(String(length=250), nullable=True)
    hmi_presentation_text = Column(String(length=500), nullable=True)
    signal_type = Column(String(length=10), nullable=True)
    group = Column(String(length=100), nullable=True)
    reaction_matrix = Column(String(length=100), nullable=True)
    external_address = Column(Integer, nullable=True)
    test_result = Column(String(length=10), nullable=True)
    tested_at = Column(DateTime(timezone=True), nullable=True)

    # Для удобной навигации SQLAlchemy
    revision = relationship("SignalListRevision", back_populates="entries")
