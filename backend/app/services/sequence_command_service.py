from __future__ import annotations

from typing import Any
from typing import Optional

from app.core.sequence_dto import SequenceCommand, SequenceCommandType
from app.infrastructure.redis.stream_bus import enqueue_sequence_command


class SequenceCommandService:
    """High-level helpers to enqueue sequence control commands into Redis streams."""

    @staticmethod
    async def enqueue_start(
        sequence_id: int,
        requested_by: Optional[str] = None,
        *,
        run_id: Optional[int] = None,
        extra: dict[str, Any] | None = None,
    ) -> SequenceCommand:
        command = SequenceCommand(
            type=SequenceCommandType.START,
            sequence_id=sequence_id,
            run_id=run_id,
            requested_by=requested_by,
            extra=extra or {},
        )
        await enqueue_sequence_command(command)
        return command

    @staticmethod
    async def enqueue_stop(
        sequence_id: int,
        requested_by: Optional[str] = None,
        *,
        run_id: Optional[int] = None,
        extra: dict[str, Any] | None = None,
    ) -> SequenceCommand:
        command = SequenceCommand(
            type=SequenceCommandType.STOP,
            sequence_id=sequence_id,
            run_id=run_id,
            requested_by=requested_by,
            extra=extra or {},
        )
        await enqueue_sequence_command(command)
        return command
