from __future__ import annotations

from datetime import datetime
from enum import Enum
from typing import TYPE_CHECKING

from sqlalchemy import DateTime, Enum as SAEnum, String, Text
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK


class SequenceStepType(str, Enum):
    WAIT = "WAIT"
    DO_LATCH = "DO_LATCH"
    DO_PULSE = "DO_PULSE"
    DO_PAIR = "DO_PAIR"
    DO_BITMASK = "DO_BITMASK"
    AO_SET = "AO_SET"


if TYPE_CHECKING:
    from app.models.sequence_run import SequenceRun
    from app.models.sequence_step import SequenceStep


class Sequence(Base):
    __tablename__ = "sequences"

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String, nullable=False)
    description: Mapped[str | None] = mapped_column(Text, nullable=True)
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    steps: Mapped[list["SequenceStep"]] = relationship(
        "SequenceStep",
        back_populates="sequence",
        cascade="all, delete-orphan",
        order_by="SequenceStep.order_index",
        lazy="selectin",
    )

    runs: Mapped[list["SequenceRun"]] = relationship(
        "SequenceRun",
        back_populates="sequence",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
