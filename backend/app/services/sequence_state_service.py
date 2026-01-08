from __future__ import annotations

from datetime import datetime, timezone
from typing import Optional

from sqlalchemy import select
from sqlalchemy.orm import selectinload

from app.infrastructure.db.database import AsyncSessionLocal
from app.models.channel import Channel
from app.models.sequence import Sequence, SequenceStep
from app.models.sequence_run import SequenceRun, SequenceRunStatus, SequenceRunStepStatus
from app.schemas.sequence_run_schema import SequenceStateSchema
from app.services.sequence_runner import SequenceNotFoundError


class SequenceStateService:
    """Read-only helper that reconstructs sequence runtime state from the database."""

    @staticmethod
    async def get_state(sequence_id: int) -> SequenceStateSchema:
        async with AsyncSessionLocal() as session:
            sequence = await SequenceStateService._load_sequence(session, sequence_id)
            if not sequence:
                raise SequenceNotFoundError(f"Sequence {sequence_id} not found")

            total_steps = len(sequence.steps)
            sequence_updated_at = sequence.updated_at or sequence.created_at

            stmt = (
                select(SequenceRun)
                .options(selectinload(SequenceRun.steps))
                .where(SequenceRun.sequence_id == sequence_id)
                .order_by(SequenceRun.started_at.desc())
                .limit(1)
            )
            result = await session.execute(stmt)
            run: Optional[SequenceRun] = result.scalar_one_or_none()

            if run:
                reference = run.finished_at or run.started_at or datetime.min.replace(tzinfo=timezone.utc)
                if sequence_updated_at and reference and sequence_updated_at > reference:
                    run = None

            if not run:
                return SequenceStateSchema(
                    sequence_id=sequence_id,
                    status="idle",
                    run_id=None,
                    current_step_index=0,
                    total_steps=total_steps,
                    completed_step_ids=[],
                    last_error=None,
                    started_at=None,
                    finished_at=None,
                )

            completed_ids = [
                step.sequence_step_id
                for step in run.steps
                if step.status == SequenceRunStepStatus.COMPLETED
            ]
            status = run.status.value if isinstance(run.status, SequenceRunStatus) else str(run.status)
            return SequenceStateSchema(
                sequence_id=sequence_id,
                status=status,
                run_id=run.id,
                current_step_index=run.current_step_index,
                total_steps=total_steps,
                completed_step_ids=completed_ids,
                last_error=run.error_message,
                started_at=run.started_at,
                finished_at=run.finished_at,
            )

    @staticmethod
    async def _load_sequence(session, sequence_id: int) -> Optional[Sequence]:
        stmt = (
            select(Sequence)
            .options(
                selectinload(Sequence.steps)
                .selectinload(SequenceStep.channel)
                .selectinload(Channel.device)
            )
            .where(Sequence.id == sequence_id)
        )
        result = await session.execute(stmt)
        return result.scalar_one_or_none()
