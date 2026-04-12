from __future__ import annotations

from typing import Any, Dict, List, Optional

from app.core.sequence_dto import SequenceEvent, SequenceEventType
from app.infrastructure.redis.stream_bus import append_sequence_event
from app.schemas.sequence_run_schema import SequenceRuntimeSchema


class SequenceEventStream:
    """Utility helpers to emit sequence lifecycle events into the Redis stream."""

    @staticmethod
    async def started(
        sequence_id: int,
        run_id: int,
        total_steps: int,
        request_id: Optional[str] = None,
        requested_by: Optional[str] = None,
        runtime: Optional[SequenceRuntimeSchema] = None,
    ) -> None:
        await SequenceEventStream._emit(
            SequenceEvent(
                type=SequenceEventType.STARTED,
                sequence_id=sequence_id,
                run_id=run_id,
                data={
                    "total_steps": total_steps,
                    "request_id": request_id,
                    "requested_by": requested_by,
                    "runtime": runtime.model_dump() if runtime else None,
                },
            )
        )

    @staticmethod
    async def stopping(
        sequence_id: int,
        run_id: int,
        current_step_index: int,
        total_steps: int,
        runtime: Optional[SequenceRuntimeSchema] = None,
    ) -> None:
        await SequenceEventStream._emit(
            SequenceEvent(
                type=SequenceEventType.STOPPING,
                sequence_id=sequence_id,
                run_id=run_id,
                data={
                    "current_step_index": current_step_index,
                    "total_steps": total_steps,
                    "runtime": runtime.model_dump() if runtime else None,
                },
            )
        )

    @staticmethod
    async def step_started(
        sequence_id: int,
        run_id: int,
        step_index: int,
        step_id: int,
        step_type: str,
    ) -> None:
        await SequenceEventStream._emit(
            SequenceEvent(
                type=SequenceEventType.STEP_STARTED,
                sequence_id=sequence_id,
                run_id=run_id,
                data={
                    "step_index": step_index,
                    "step_id": step_id,
                    "step_type": step_type,
                },
            )
        )

    @staticmethod
    async def step_completed(
        sequence_id: int,
        run_id: int,
        step_index: int,
        step_id: int,
        step_type: str,
        progress_scope: str,
        step_elapsed_ms: int,
        run_elapsed_ms: int,
        completed_step_ids: List[int],
        runtime: Optional[SequenceRuntimeSchema] = None,
    ) -> None:
        await SequenceEventStream._emit(
            SequenceEvent(
                type=SequenceEventType.STEP_COMPLETED,
                sequence_id=sequence_id,
                run_id=run_id,
                data={
                    "step_index": step_index,
                    "step_id": step_id,
                    "step_type": step_type,
                    "progress_scope": progress_scope,
                    "step_elapsed_ms": step_elapsed_ms,
                    "run_elapsed_ms": run_elapsed_ms,
                    "completed_step_ids": completed_step_ids,
                    "runtime": runtime.model_dump() if runtime else None,
                },
            )
        )

    @staticmethod
    async def finished(
        sequence_id: int,
        run_id: int,
        status: str,
        elapsed_ms: int,
        current_step_index: Optional[int] = None,
        total_steps: Optional[int] = None,
        runtime: Optional[SequenceRuntimeSchema] = None,
    ) -> None:
        payload: Dict[str, Any] = {
            "status": status,
            "elapsed_ms": elapsed_ms,
        }
        if current_step_index is not None:
            payload["current_step_index"] = current_step_index
        if total_steps is not None:
            payload["total_steps"] = total_steps
        if runtime is not None:
            payload["runtime"] = runtime.model_dump()

        await SequenceEventStream._emit(
            SequenceEvent(
                type=SequenceEventType.FINISHED,
                sequence_id=sequence_id,
                run_id=run_id,
                data=payload,
            )
        )

    @staticmethod
    async def failed(
        sequence_id: int,
        run_id: int,
        message: str,
        step_index: Optional[int] = None,
        step_id: Optional[int] = None,
        runtime: Optional[SequenceRuntimeSchema] = None,
    ) -> None:
        await SequenceEventStream._emit(
            SequenceEvent(
                type=SequenceEventType.FAILED,
                sequence_id=sequence_id,
                run_id=run_id,
                data={
                    "message": message,
                    "step_index": step_index,
                    "step_id": step_id,
                    "runtime": runtime.model_dump() if runtime else None,
                },
            )
        )

    @staticmethod
    async def _emit(event: SequenceEvent) -> None:
        await append_sequence_event(event)
