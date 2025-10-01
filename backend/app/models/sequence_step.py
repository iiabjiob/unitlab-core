from __future__ import annotations
from typing import TYPE_CHECKING
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import BigInteger, Integer, String, ForeignKey, JSON
from app.infrastructure.db.database import Base

if TYPE_CHECKING:
    from app.models.sequence import Sequence
    from app.models.channel import Channel

class SequenceStep(Base):
    __tablename__ = "sequence_steps"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    sequence_id: Mapped[int] = mapped_column(
        BigInteger, ForeignKey("sequences.id", ondelete="CASCADE"), nullable=False
    )
    order_index: Mapped[int] = mapped_column(Integer, nullable=False)
    kind: Mapped[str] = mapped_column(String, nullable=False)  # e.g. DO_SET, WAIT
    channel_id: Mapped[int | None] = mapped_column(
        BigInteger, ForeignKey("channels.id", ondelete="SET NULL"), nullable=True
    )
    payload: Mapped[dict | None] = mapped_column(JSON)

    sequence: Mapped["Sequence"] = relationship("Sequence", back_populates="steps")
    channel: Mapped["Channel"] = relationship("Channel", back_populates="steps")
