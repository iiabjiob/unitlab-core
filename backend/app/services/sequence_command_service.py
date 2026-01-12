from __future__ import annotations

from typing import Optional

from app.core.sequence_dto import SequenceCommand, SequenceCommandType
from app.infrastructure.redis.stream_bus import enqueue_sequence_command


class SequenceCommandService:
    """High-level helpers to enqueue sequence control commands into Redis streams."""

    @staticmethod
    async def enqueue_start(test_run_id: int, requested_by: Optional[str] = None) -> SequenceCommand:
        command = SequenceCommand(
            type=SequenceCommandType.START,
            test_run_id=test_run_id,
            requested_by=requested_by,
        )
        await enqueue_sequence_command(command)
        return command

    @staticmethod
    async def enqueue_stop(test_run_id: int, requested_by: Optional[str] = None) -> SequenceCommand:
        command = SequenceCommand(
            type=SequenceCommandType.STOP,
            test_run_id=test_run_id,
            requested_by=requested_by,
        )
        await enqueue_sequence_command(command)
        return command
