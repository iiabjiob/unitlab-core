from __future__ import annotations

from datetime import datetime

from sqlalchemy import Boolean, DateTime, String, Text
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy.sql import func, expression

from app.infrastructure.db.database import Base
from app.models.types import BIGINT_PK

class Sequence(Base):
    __tablename__: str = "sequences"
    __table_args__: tuple[()] = ()

    id: Mapped[int] = mapped_column(BIGINT_PK, primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String, nullable=False)
    description: Mapped[str | None] = mapped_column(Text, nullable=True)
    system_key: Mapped[str | None] = mapped_column(String(255), unique=True, nullable=True)
    system_provided: Mapped[bool] = mapped_column(
        Boolean,
        nullable=False,
        server_default=expression.false(),
        default=False,
    )
    read_only: Mapped[bool] = mapped_column(
        Boolean,
        nullable=False,
        server_default=expression.false(),
        default=False,
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    steps: Mapped[list[object]] = relationship(
        "SequenceStep",
        back_populates="sequence",
        cascade="all, delete-orphan",
        order_by="SequenceStep.order_index",
        lazy="selectin",
    )

    runs: Mapped[list[object]] = relationship(
        "SequenceRun",
        back_populates="sequence",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    workspace_links: Mapped[list[object]] = relationship(
        "WorkspaceSequence",
        back_populates="sequence",
        cascade="all, delete-orphan",
        lazy="selectin",
    )
    workspaces: Mapped[list[object]] = relationship(
        "Workspace",
        secondary="workspace_sequences",
        viewonly=True,
        lazy="selectin",
    )
