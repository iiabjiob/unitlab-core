from __future__ import annotations

from app.core.sequence_dto import SequenceCommand, SequenceCommandType
from app.infrastructure.redis.stream_bus import enqueue_sequence_command


class SequenceCommandService:
    """High-level helpers to enqueue sequence control commands into Redis streams."""

    @staticmethod
    async def enqueue_start(
        sequence_id: int,
        requested_by: str | None = None,
        *,
        workspace_id: int | None = None,
        run_id: int | None = None,
        extra: dict[str, object] | None = None,
    ) -> SequenceCommand:
        command = SequenceCommand(
            type=SequenceCommandType.START,
            sequence_id=sequence_id,
            run_id=run_id,
            requested_by=requested_by,
            extra={**(extra or {}), **({"workspace_id": workspace_id} if workspace_id is not None else {})},
        )
        _ = await enqueue_sequence_command(command)
        return command

    @staticmethod
    async def enqueue_stop(
        sequence_id: int,
        requested_by: str | None = None,
        *,
        run_id: int | None = None,
        extra: dict[str, object] | None = None,
    ) -> SequenceCommand:
        command = SequenceCommand(
            type=SequenceCommandType.STOP,
            sequence_id=sequence_id,
            run_id=run_id,
            requested_by=requested_by,
            extra=extra or {},
        )
        _ = await enqueue_sequence_command(command)
        return command
