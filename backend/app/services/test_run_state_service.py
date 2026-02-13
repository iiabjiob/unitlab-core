from __future__ import annotations

from datetime import datetime, timezone

from sqlalchemy.ext.asyncio import AsyncSession

from app.models.test_run import TestRun, TestRunStatus
from app.services.sequence_runner import SequenceNotFoundError
from app.services.sequence_state_service import SequenceStateService

_TERMINAL_SUCCESS_STATES = {"completed"}
_TERMINAL_FAILURE_STATES = {"error", "stopped"}
_RUNNING_STATES = {"running", "cancelling", "pending"}


class TestRunStateService:
    """Synchronize test-run status from current sequence runtime states."""

    @staticmethod
    async def sync_status(db: AsyncSession, run: TestRun) -> None:
        if run.status not in {TestRunStatus.RUNNING, TestRunStatus.CREATED}:
            return

        sequence_ids = [link.sequence_id for link in sorted(run.sequence_links, key=lambda item: item.order_index)]
        if not sequence_ids:
            return

        states: dict[int, str] = {}
        for sequence_id in sequence_ids:
            try:
                state = await SequenceStateService.get_state(sequence_id)
            except SequenceNotFoundError:
                states[sequence_id] = "missing"
                continue
            states[sequence_id] = state.status

        status_values = set(states.values())

        if status_values & _TERMINAL_FAILURE_STATES or "missing" in status_values:
            run.status = TestRunStatus.FAILED
            run.finished_at = run.finished_at or datetime.now(timezone.utc)
        elif status_values and status_values.issubset(_TERMINAL_SUCCESS_STATES):
            run.status = TestRunStatus.COMPLETED
            run.finished_at = run.finished_at or datetime.now(timezone.utc)
            TestRunStateService._stamp_tested_at(run)
        elif status_values & _RUNNING_STATES:
            run.status = TestRunStatus.RUNNING
            run.started_at = run.started_at or datetime.now(timezone.utc)

        meta = dict(run.execution_meta or {})
        meta["sequence_states"] = states
        run.execution_meta = meta

        await db.flush()

    @staticmethod
    def _stamp_tested_at(run: TestRun) -> None:
        if not run.allocation:
            return

        tested_at = datetime.now(timezone.utc).isoformat()
        for entry in run.allocation.entries:
            payload = dict(entry.signal_metadata or {})
            payload.setdefault("tested_at", tested_at)
            entry.signal_metadata = payload
