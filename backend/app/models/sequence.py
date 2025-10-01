from __future__ import annotations
from datetime import datetime
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import String, Text, DateTime, BigInteger
from sqlalchemy.sql import func
from typing import TYPE_CHECKING
from app.infrastructure.db.database import Base

if TYPE_CHECKING:
    from app.models.sequence_step import SequenceStep

class Sequence(Base):
    __tablename__ = "sequences"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String, nullable=False)
    description: Mapped[str | None] = mapped_column(Text)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    steps: Mapped[list["SequenceStep"]] = relationship(
        "SequenceStep",
        back_populates="sequence",
        cascade="all, delete-orphan",
        order_by="SequenceStep.order_index",
    )
