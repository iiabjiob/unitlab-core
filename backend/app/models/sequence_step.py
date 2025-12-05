from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from sqlalchemy import (
    DateTime,
    ForeignKey,
    Integer,
    JSON,
    UniqueConstraint,
    Index,
    Enum as SAEnum,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.sequence import SequenceStepType
from app.models.types import BIGINT_PK

if TYPE_CHECKING:
    from app.models.channel import Channel
    from app.models.sequence import Sequence


class SequenceStep(Base):
    __tablename__ = "sequence_steps"

    __table_args__ = (
        UniqueConstraint("sequence_id", "order_index", name="uq_sequence_steps_order"),
        Index("ix_sequence_steps_seq_order", "sequence_id", "order_index"),
    )

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    sequence_id: Mapped[int] = mapped_column(
        BIGINT_PK, ForeignKey("sequences.id", ondelete="CASCADE"), nullable=False
    )
    order_index: Mapped[int] = mapped_column(Integer, nullable=False)
    sequence_step_type: Mapped[SequenceStepType] = mapped_column(
        SAEnum(SequenceStepType, name="sequence_step_type_enum"),
        nullable=False,
    )
    channel_id: Mapped[int | None] = mapped_column(
        BIGINT_PK, ForeignKey("channels.id", ondelete="SET NULL"), nullable=True
    )
    payload: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )

    sequence: Mapped["Sequence"] = relationship(
        "Sequence", back_populates="steps", lazy="selectin"
    )
    channel: Mapped["Channel"] = relationship("Channel", back_populates="steps", lazy="selectin")
