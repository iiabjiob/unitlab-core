from __future__ import annotations

from datetime import datetime, timezone

from app.api.v1.test_runs import TestRunsRepository
from app.models.test_run import TestRun, TestRunStatus
from app.schemas.sequence_run_schema import SequenceStateSchema
from app.services.sequence_command_service import SequenceCommandService
from app.services.sequence_state_service import SequenceStateService
from app.services.signal_binding_service import SignalBindingService


class TestRunConflictError(Exception):
    """Raised when a test run cannot transition due to current runtime state."""


class TestRunOrchestrator:
    def __init__(self, repo: TestRunsRepository):
        self.repo = repo

    async def start(self, run: TestRun) -> list[SequenceStateSchema]:
        if run.status == TestRunStatus.RUNNING:
            raise TestRunConflictError("Test run is already running")

        sequence_ids = [link.sequence_id for link in sorted(run.sequence_links, key=lambda item: item.order_index)]
        if not sequence_ids:
            raise TestRunConflictError("Test run has no sequences")

        states_before = await self._collect_states(sequence_ids)
        if any(state.status in {"running", "cancelling", "pending"} for state in states_before):
            raise TestRunConflictError("One or more sequences are already running")

        allocation_entries = run.allocation.entries if run.allocation else []
        bindings = SignalBindingService.build_bindings(allocation_entries)

        execution_meta = dict(run.execution_meta or {})
        execution_meta.update(
            {
                "started_at": datetime.now(timezone.utc).isoformat(),
                "sequence_ids": sequence_ids,
                "signal_bindings": bindings,
            }
        )

        await self.repo.create_signal_snapshot(run)

        await self.repo.mark_running(run, execution_meta)

        for sequence_id in sequence_ids:
            await SequenceCommandService.enqueue_start(
                sequence_id,
                run_id=run.id,
                extra={"signal_bindings": bindings},
            )

        await self.repo.db.commit()

        return await self._collect_states(sequence_ids)

    async def stop(self, run: TestRun) -> list[SequenceStateSchema]:
        sequence_ids = [link.sequence_id for link in sorted(run.sequence_links, key=lambda item: item.order_index)]
        if not sequence_ids:
            raise TestRunConflictError("Test run has no sequences")

        for sequence_id in sequence_ids:
            await SequenceCommandService.enqueue_stop(sequence_id, run_id=run.id)

        meta = dict(run.execution_meta or {})
        meta["stop_requested_at"] = datetime.now(timezone.utc).isoformat()
        await self.repo.mark_failed(run, meta)
        await self.repo.db.commit()

        return await self._collect_states(sequence_ids)

    async def _collect_states(self, sequence_ids: list[int]) -> list[SequenceStateSchema]:
        states: list[SequenceStateSchema] = []
        for sequence_id in sequence_ids:
            states.append(await SequenceStateService.get_state(sequence_id))
        return states
