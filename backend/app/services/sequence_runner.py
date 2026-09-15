from __future__ import annotations

import asyncio
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any, Awaitable, Callable, Dict, List, Optional, Set

from sqlalchemy import select, update
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import selectinload

from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.models.channel import Channel
from app.models.device import Device
from app.models.switchgear import Switchgear
from app.models.sequence import Sequence, SequenceStep, SequenceStepType
from app.models.sequence_run import (
    SequenceRun,
    SequenceRunStatus,
    SequenceRunStep,
    SequenceRunStepStatus,
)
from app.models.workspace import WorkspaceSequence, WorkspaceSwitchgear
from app.schemas.sequence_run_schema import SequenceRuntimeSchema, SequenceStateSchema
from app.services.domain_errors import (
    ChannelNotFoundError,
    SequenceDeviceUnavailableError,
    SequenceNotApplicableError,
    SequenceStepBlockedError,
)
from app.services.sequence_executor import (
    CancellationEvent,
    ChannelInfo,
    DeviceInfo,
    RunStartedEvent,
    SequenceCancellationRequested,
    SequenceExecutionResult,
    SequenceExecutor,
    HardwareCommandAdmission as HardwareCommandAdmissionFn,
    SequenceExecutorHooks,
    StepCompletedEvent,
    StepContext,
    StepFailedEvent,
    StepLifecycleEvent,
)
from app.services.sequence_event_stream import SequenceEventStream
from app.services.hardware_command_ack import wait_for_hardware_command_acks
from app.services.hardware_command_admission import HardwareCommandAdmission
from app.services.hardware_command_intent import (
    mark_hardware_command_intent_delivery_failure,
    mark_hardware_command_intent_completed,
    mark_hardware_command_intent_queued,
    list_hardware_recovery_required_channels,
    reconcile_unfinished_hardware_command_intents,
    record_hardware_command_intent,
)
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.protocol.modes import State
from app.services.command_queue_service import enqueue_request_state
from app.services.device_presence_service import DevicePresenceService
from uuid import uuid4


logger = get_logger("sequence.runner")

SEQUENCE_READBACK_TIMEOUT_MS = 2000
SEQUENCE_HARDWARE_STEP_TIMEOUT_MS = 7000


async def _wait_for_sequence_readback(
    redis: Any,
    *,
    unit_id: str,
    targets: list[tuple[int, int | float]],
    analog: bool,
    packet_id: int,
    timeout_ms: int = SEQUENCE_READBACK_TIMEOUT_MS,
    cancel_event: asyncio.Event | None = None,
    cancellation_probe: Optional[Callable[[], Awaitable[None]]] = None,
) -> bool:
    """Require a state snapshot caused by this request before accepting a step."""
    if not targets:
        return False
    deadline = time.monotonic() + max(100, int(timeout_ms)) / 1000
    while True:
        if cancellation_probe is not None:
            await cancellation_probe()
        if cancel_event is not None and cancel_event.is_set():
            raise SequenceCancellationRequested()
        try:
            fresh = int(await redis.get(f"device:{unit_id}:last_state_packet_id")) == int(packet_id)
        except (TypeError, ValueError):
            fresh = False
        if fresh:
            matched = True
            if analog:
                for channel_index, expected in targets:
                    try:
                        actual = float(await redis.hget(f"device:{unit_id}:ao", str(int(channel_index))))
                    except (TypeError, ValueError):
                        matched = False
                        break
                    if abs(actual - float(expected)) > 0.01:
                        matched = False
                        break
            else:
                try:
                    bitmask = int(await redis.get(f"device:{unit_id}:bitmask"))
                except (TypeError, ValueError):
                    bitmask = None
                if bitmask is None:
                    matched = False
                else:
                    matched = all(
                        (1 if bitmask & (1 << int(channel_index)) else 0) == int(expected)
                        for channel_index, expected in targets
                    )
            if matched:
                return True
        if time.monotonic() >= deadline:
            return False
        if cancel_event is None:
            await asyncio.sleep(0.05)
        else:
            try:
                await asyncio.wait_for(cancel_event.wait(), timeout=0.05)
            except asyncio.TimeoutError:
                pass


def _sequence_readback_targets(
    *,
    action: str,
    payload: dict[str, Any],
) -> tuple[list[tuple[int, int | float]], bool]:
    if action == "ao_set":
        return [(int(payload["channel_index"]), float(payload["value"]))], True
    if action == "do_set" or action == "do_pulse":
        return [(int(payload["channel_index"]), int(payload["value"]))], False
    if action == "do_pair":
        state2b = int(payload["state2b"]) & 0b11
        indexes = [int(index) for index in payload["channel_indexes"]]
        if len(indexes) != 2:
            raise SequenceNotApplicableError("DO_PAIR readback requires two channel indexes")
        return [(indexes[0], state2b & 0b01), (indexes[1], (state2b >> 1) & 0b01)], False
    if action == "do_all":
        bitmask = int(payload["bitmask"])
        indexes = payload.get("channel_indexes")
        if not isinstance(indexes, list) or not indexes:
            raise SequenceNotApplicableError("DO_BITMASK readback requires resolved channel indexes")
        return [
            (int(index), 1 if bitmask & (1 << int(index)) else 0)
            for index in indexes
        ], False
    raise SequenceNotApplicableError(f"Unsupported hardware readback action {action}")


class SequenceRunnerError(Exception):
    """Base class for sequence runner exceptions."""


class SequenceNotFoundError(SequenceRunnerError):
    """Raised when a target sequence cannot be located."""


class SequenceAlreadyRunningError(SequenceRunnerError):
    """Raised when attempting to start a sequence that is already active."""


@dataclass
class ActiveRun:
    sequence_id: int
    run_id: int
    cancel_event: asyncio.Event
    task: asyncio.Task[None]


@dataclass(frozen=True)
class ResolvedSequenceStep:
    sequence_id: int
    sequence_name: str
    sequence_step_id: int
    order_index: int
    total_steps: int
    step_type: SequenceStepType
    payload: dict[str, Any]
    primary_channel: Optional[ChannelInfo]
    pair_channels: List[ChannelInfo]
    target_device: Optional[DeviceInfo]
    run_step_id: Optional[int] = None


@dataclass(frozen=True)
class ResolvedSequenceDefinition:
    id: int
    name: str
    steps: List[ResolvedSequenceStep]


@dataclass(frozen=True)
class RepeatStepConfig:
    mode: str
    iterations: Optional[int] = None
    duration_ms: Optional[int] = None


@dataclass(frozen=True)
class ExecutionLoopState:
    current: int
    total: Optional[int]
    mode: str


@dataclass(frozen=True)
class RuntimeCursor:
    active_sequence_id: int
    active_sequence_name: str
    active_step_id: int
    active_step_index: int
    active_total_steps: int
    active_step_type: str
    execution_path: tuple[str, ...]
    loop_state: Optional[ExecutionLoopState] = None

    def to_schema(self, *, start_time: float) -> SequenceRuntimeSchema:
        return SequenceRuntimeSchema(
            execution_path=list(self.execution_path),
            active_sequence_id=self.active_sequence_id,
            active_sequence_name=self.active_sequence_name,
            active_step_id=self.active_step_id,
            active_step_index=self.active_step_index,
            active_total_steps=self.active_total_steps,
            active_step_type=self.active_step_type,
            iteration_current=self.loop_state.current if self.loop_state else None,
            iteration_total=self.loop_state.total if self.loop_state else None,
            repeat_mode=self.loop_state.mode if self.loop_state else None,
            run_elapsed_ms=int((time.monotonic() - start_time) * 1000),
        )


class SequenceRunner:
    """Coordinates asynchronous sequence executions directly from stored sequences."""

    _CANCELLATION_PROBE_MIN_INTERVAL = 0.3  # seconds

    def __init__(self) -> None:
        self._active_runs: Dict[int, ActiveRun] = {}
        self._state_cache: Dict[int, SequenceStateSchema] = {}
        self._last_cancellation_probe_at: Dict[int, float] = {}
        self._lock = asyncio.Lock()
        self._stopping_emitted_runs: Set[int] = set()
        self._executor = SequenceExecutor()

    async def start(
        self,
        sequence_id: int,
        *,
        workspace_id: Optional[int] = None,
        request_id: Optional[str] = None,
        requested_by: Optional[str] = None,
        signal_bindings: Optional[dict[str, int]] = None,
    ) -> SequenceStateSchema:
        if workspace_id is None or int(workspace_id) <= 0:
            raise SequenceNotApplicableError("Sequence start requires workspace_id")
        self.invalidate_state(sequence_id)

        async with self._lock:
            if sequence_id in self._active_runs:
                raise SequenceAlreadyRunningError(f"Sequence {sequence_id} already running")

            run_id, root_sequence, resolved_sequences = await self._create_run(
                sequence_id,
                workspace_id=workspace_id,
                signal_bindings=signal_bindings or {},
            )
            cancel_event = asyncio.Event()
            task = asyncio.create_task(
                self._execute_run(
                    sequence_id=sequence_id,
                    run_id=run_id,
                    root_sequence=root_sequence,
                    resolved_sequences=resolved_sequences,
                    cancel_event=cancel_event,
                    request_id=request_id,
                    requested_by=requested_by,
                    workspace_id=workspace_id,
                ),
                name=f"sequence-run-{run_id}",
            )
            self._active_runs[sequence_id] = ActiveRun(
                sequence_id=sequence_id,
                run_id=run_id,
                cancel_event=cancel_event,
                task=task,
            )
            task.add_done_callback(lambda _: self._active_runs.pop(sequence_id, None))
            logger.info("Sequence %s run %s scheduled", sequence_id, run_id)

        return await self.get_state(sequence_id)

    async def stop(self, sequence_id: int) -> SequenceStateSchema:
        self.invalidate_state(sequence_id)
        handle: Optional[ActiveRun] = None
        async with self._lock:
            handle = self._active_runs.get(sequence_id)

        run_id: Optional[int] = None
        if handle:
            run_id = handle.run_id
        else:
            run_id = await self._get_active_run_id(sequence_id)

        if run_id is not None:
            await self._transition_to_cancelling(sequence_id, run_id)

        if handle:
            handle.cancel_event.set()
            try:
                await asyncio.wait_for(handle.task, timeout=5)
            except asyncio.TimeoutError:
                logger.warning("Timed out waiting for sequence %s stop", sequence_id)
                handle.task.cancel()
                try:
                    await handle.task
                except BaseException:  # noqa: BLE001
                    pass
                await self._finalize_orphaned_stop(sequence_id, run_id)
        else:
            await self._finalize_orphaned_stop(sequence_id, run_id)

        return await self.get_state(sequence_id)

    async def _finalize_orphaned_stop(self, sequence_id: int, run_id: Optional[int]) -> None:
        if run_id is None:
            return

        async with AsyncSessionLocal() as session:
            result = await session.execute(
                select(
                    SequenceRun.status,
                    SequenceRun.current_step_index,
                    SequenceRun.started_at,
                ).where(SequenceRun.id == run_id)
            )
            row = result.one_or_none()
            if row is None or row[0] not in (
                SequenceRunStatus.PENDING,
                SequenceRunStatus.RUNNING,
                SequenceRunStatus.CANCELLING,
            ):
                return
            current_step_index = int(row[1] or 0)
            started_at = row[2]
            await session.execute(
                update(SequenceRunStep)
                .where(
                    SequenceRunStep.run_id == run_id,
                    SequenceRunStep.status == SequenceRunStepStatus.RUNNING,
                )
                .values(
                    status=SequenceRunStepStatus.CANCELLED,
                    finished_at=datetime.now(timezone.utc),
                    error_message="stopped",
                )
            )
            await self._record_cancellation(
                session=session,
                run_id=run_id,
                current_step=None,
                step_started_monotonic=None,
                fallback_index=current_step_index,
            )
            await session.commit()

        self.invalidate_state(sequence_id)
        state = await self.get_state(sequence_id)
        elapsed_since_start = max(
            0.0,
            (datetime.now(timezone.utc) - started_at).total_seconds(),
        )
        await self._publish_terminal_state(
            sequence_id=sequence_id,
            run_id=run_id,
            status="stopped",
            current_step_index=state.current_step_index,
            total_steps=state.total_steps,
            completed_step_ids=list(state.completed_step_ids),
            started_at=started_at,
            start_time=time.monotonic() - elapsed_since_start,
            last_error="stopped",
        )

    async def get_state(self, sequence_id: int) -> SequenceStateSchema:
        cached = self._state_cache.get(sequence_id)
        if cached:
            return cached

        async with AsyncSessionLocal() as session:
            sequence = await self._load_sequence(session, sequence_id, include_steps=True)
            if not sequence:
                raise SequenceNotFoundError(f"Sequence {sequence_id} not found")

            stmt = (
                select(SequenceRun)
                .options(selectinload(SequenceRun.steps))
                .where(SequenceRun.sequence_id == sequence_id)
                .order_by(SequenceRun.started_at.desc())
                .limit(1)
            )
            result = await session.execute(stmt)
            run: Optional[SequenceRun] = result.scalar_one_or_none()

            total_steps = len(sequence.steps)
            sequence_updated_at = sequence.updated_at or sequence.created_at

            if run:
                reference = run.finished_at or run.started_at or datetime.min.replace(tzinfo=timezone.utc)
                if sequence_updated_at and reference and sequence_updated_at > reference:
                    run = None

            if not run:
                state = SequenceStateSchema(
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
            else:
                await session.refresh(run)
                completed_ids = [
                    step.sequence_step_id
                    for step in run.steps
                    if step.status == SequenceRunStepStatus.COMPLETED
                ]
                state = SequenceStateSchema(
                    sequence_id=sequence_id,
                    status=run.status.value if isinstance(run.status, SequenceRunStatus) else str(run.status),
                    run_id=run.id,
                    current_step_index=run.current_step_index,
                    total_steps=total_steps,
                    completed_step_ids=completed_ids,
                    last_error=run.error_message,
                    started_at=run.started_at,
                    finished_at=run.finished_at,
                )

            self._state_cache[sequence_id] = state
            return state


    async def _activate_run(self, session, run_id: int, started_at: datetime) -> bool:
        stmt = (
            update(SequenceRun)
            .where(
                SequenceRun.id == run_id,
                SequenceRun.status == SequenceRunStatus.PENDING,
            )
            .values(status=SequenceRunStatus.RUNNING, started_at=started_at)
            .returning(SequenceRun.id)
        )
        result = await session.execute(stmt)
        activated = result.scalar_one_or_none() is not None
        return activated

    async def _handle_activation_skip(
        self,
        *,
        session,
        sequence_id: int,
        run_id: int,
        total_steps: int,
        completed_step_ids: List[int],
        started_at: datetime,
        start_time: float,
    ) -> Optional[int]:
        status = await session.scalar(
            select(SequenceRun.status).where(SequenceRun.id == run_id)
        )
        if status == SequenceRunStatus.CANCELLING:
            return await self._record_cancellation(
                session=session,
                run_id=run_id,
                current_step=None,
                step_started_monotonic=None,
                fallback_index=len(completed_step_ids),
            )
        elif status is None:
            logger.warning("Run %s vanished before activation", run_id)
        else:
            logger.info(
                "Skipping activation for sequence %s run %s (status=%s)",
                sequence_id,
                run_id,
                status,
            )
        return None

    async def _publish_terminal_state(
        self,
        *,
        sequence_id: int,
        run_id: int,
        status: str,
        current_step_index: int,
        total_steps: int,
        completed_step_ids: List[int],
        started_at: datetime,
        start_time: float,
        last_error: Optional[str] = None,
        runtime: Optional[SequenceRuntimeSchema] = None,
        blocked_step_ids: Optional[List[int]] = None,
    ) -> None:
        elapsed_total = int((time.monotonic() - start_time) * 1000)
        finished_at = datetime.now(timezone.utc)
        snapshot = list(completed_step_ids)
        if status == "stopped":
            await self._ensure_stopping_event(
                sequence_id=sequence_id,
                run_id=run_id,
                current_step_index=current_step_index,
                total_steps=total_steps,
            )
        await SequenceEventStream.finished(
            sequence_id=sequence_id,
            run_id=run_id,
            status=status,
            elapsed_ms=elapsed_total,
            current_step_index=current_step_index,
            total_steps=total_steps,
        )
        self._cache_state(
            sequence_id,
            SequenceStateSchema(
                sequence_id=sequence_id,
                status=status,
                run_id=run_id,
                current_step_index=current_step_index,
                total_steps=total_steps,
                completed_step_ids=snapshot,
                blocked_step_ids=list(blocked_step_ids or []),
                last_error=last_error,
                started_at=started_at,
                finished_at=finished_at,
                runtime=runtime,
            ),
        )
        self._reset_cancellation_probe(run_id)
        self._clear_stopping_event_flag(run_id)
        if status in {"completed", "completed_with_issues"}:
            logger.info(
                "Sequence run %s completed in %sms (%s steps)",
                run_id,
                elapsed_total,
                total_steps,
            )
        elif status == "stopped":
            logger.info(
                "Sequence run %s stopped at step %s",
                run_id,
                current_step_index,
            )

    async def _publish_step_issue(
        self,
        *,
        sequence_id: int,
        run_id: int,
        step_index: int,
        step_id: int,
        status: str,
        message: str,
        runtime: Optional[SequenceRuntimeSchema],
    ) -> None:
        try:
            await asyncio.wait_for(
                SequenceEventStream.step_issue(
                    sequence_id=sequence_id,
                    run_id=run_id,
                    step_index=step_index,
                    step_id=step_id,
                    status=status,
                    message=message,
                    runtime=runtime,
                ),
                timeout=1.0,
            )
        except Exception as exc:  # noqa: BLE001
            logger.warning(
                "Failed to publish sequence step issue (sequence=%s, run=%s, step=%s): %s",
                sequence_id,
                run_id,
                step_id,
                exc,
            )

    def _reset_cancellation_probe(self, run_id: int) -> None:
        tracker = getattr(self, "_last_cancellation_probe_at", None)
        if tracker is not None:
            tracker.pop(run_id, None)

    async def _ensure_stopping_event(
        self,
        *,
        sequence_id: int,
        run_id: int,
        current_step_index: int,
        total_steps: int,
    ) -> None:
        if run_id in self._stopping_emitted_runs:
            return
        await SequenceEventStream.stopping(
            sequence_id=sequence_id,
            run_id=run_id,
            current_step_index=current_step_index,
            total_steps=total_steps,
        )
        self._stopping_emitted_runs.add(run_id)

    def _clear_stopping_event_flag(self, run_id: int) -> None:
        self._stopping_emitted_runs.discard(run_id)

    async def _record_cancellation(
        self,
        *,
        session,
        run_id: int,
        current_step: Optional[StepContext],
        step_started_monotonic: Optional[float],
        fallback_index: int,
    ) -> int:
        await reconcile_unfinished_hardware_command_intents(
            session,
            job_id=str(run_id),
        )
        if current_step and step_started_monotonic is not None:
            await self._mark_step_status(
                session,
                current_step.run_step_id,
                SequenceRunStepStatus.CANCELLED,
                step_started_monotonic,
            )
        current_index = current_step.index if current_step else fallback_index
        await self._mark_run_stopped(session, run_id, current_index)
        return current_index

    async def _on_run_started(
        self,
        *,
        sequence_id: int,
        run_id: int,
        total_steps: int,
        started_at: datetime,
        request_id: Optional[str],
        requested_by: Optional[str],
        runtime: Optional[SequenceRuntimeSchema] = None,
    ) -> None:
        await SequenceEventStream.started(
            sequence_id=sequence_id,
            run_id=run_id,
            total_steps=total_steps,
            request_id=request_id,
            requested_by=requested_by,
            runtime=runtime,
        )
        self._cache_state(
            sequence_id,
            SequenceStateSchema(
                sequence_id=sequence_id,
                status="running",
                run_id=run_id,
                current_step_index=0,
                total_steps=total_steps,
                completed_step_ids=[],
                last_error=None,
                started_at=started_at,
                finished_at=None,
                runtime=runtime,
            ),
        )
        logger.info(
            "Sequence %s run %s started (%s steps)",
            sequence_id,
            run_id,
            total_steps,
        )

    def _cache_running_state(
        self,
        *,
        sequence_id: int,
        run_id: int,
        current_step_index: int,
        total_steps: int,
        completed_step_ids: List[int],
        started_at: datetime,
        runtime: Optional[SequenceRuntimeSchema],
    ) -> None:
        self._cache_state(
            sequence_id,
            SequenceStateSchema(
                sequence_id=sequence_id,
                status="running",
                run_id=run_id,
                current_step_index=current_step_index,
                total_steps=total_steps,
                completed_step_ids=list(completed_step_ids),
                last_error=None,
                started_at=started_at,
                finished_at=None,
                runtime=runtime,
            ),
        )


    async def _set_step_running(self, session, run_step_id: int, started_at: datetime) -> None:
        await session.execute(
            update(SequenceRunStep)
            .where(SequenceRunStep.id == run_step_id)
            .values(status=SequenceRunStepStatus.RUNNING, started_at=started_at)
        )

    async def _probe_cancellation_from_db(self, run_id: int, cancel_event: asyncio.Event) -> None:
        if cancel_event.is_set():
            return
        tracker = getattr(self, "_last_cancellation_probe_at", None)
        if tracker is None:
            tracker = self._last_cancellation_probe_at = {}

        now = time.monotonic()
        last_checked = tracker.get(run_id)
        if last_checked is not None and now - last_checked < self._CANCELLATION_PROBE_MIN_INTERVAL:
            return
        tracker[run_id] = now

        async with AsyncSessionLocal() as session:
            status = await session.scalar(
                select(SequenceRun.status).where(SequenceRun.id == run_id)
            )
        if status in (SequenceRunStatus.CANCELLING, SequenceRunStatus.STOPPED):
            cancel_event.set()

    async def _get_active_run_id(self, sequence_id: int) -> Optional[int]:
        async with AsyncSessionLocal() as session:
            stmt = (
                select(SequenceRun.id)
                .where(
                    SequenceRun.sequence_id == sequence_id,
                    SequenceRun.status.in_(
                        [
                            SequenceRunStatus.PENDING,
                            SequenceRunStatus.RUNNING,
                            SequenceRunStatus.CANCELLING,
                        ]
                    ),
                )
                .order_by(SequenceRun.started_at.desc())
                .limit(1)
            )
            result = await session.execute(stmt)
            return result.scalar_one_or_none()

    async def _create_run(
        self,
        sequence_id: int,
        *,
        workspace_id: Optional[int] = None,
        signal_bindings: dict[str, int] | None = None,
    ) -> tuple[int, ResolvedSequenceDefinition, dict[int, ResolvedSequenceDefinition]]:
        signal_bindings = signal_bindings or {}
        async with AsyncSessionLocal() as session:
            sequence = await self._load_sequence(session, sequence_id, include_steps=True)
            if not sequence:
                raise SequenceNotFoundError(f"Sequence {sequence_id} not found")
            if workspace_id is not None and await session.scalar(
                select(WorkspaceSequence.workspace_id).where(
                    WorkspaceSequence.workspace_id == workspace_id,
                    WorkspaceSequence.sequence_id == sequence_id,
                )
            ) is None:
                raise SequenceNotApplicableError("Sequence is not linked to the requested workspace")

            ordered_steps = sorted(sequence.steps, key=lambda s: s.order_index)
            if not ordered_steps:
                raise SequenceNotApplicableError(f"Sequence {sequence_id} has no steps to execute")

            run = SequenceRun(
                sequence_id=sequence.id,
                status=SequenceRunStatus.PENDING,
                current_step_index=0,
            )
            run.steps = [
                SequenceRunStep(
                    sequence_step_id=step.id,
                    order_index=index,
                    status=SequenceRunStepStatus.PENDING,
                )
                for index, step in enumerate(ordered_steps)
            ]
            session.add(run)

            try:
                await session.flush()
                resolved_cache: dict[int, ResolvedSequenceDefinition] = {}
                resolved_root = await self._build_resolved_sequence(
                    session,
                    sequence_id,
                    workspace_id=workspace_id,
                    signal_bindings=signal_bindings,
                    resolved_cache=resolved_cache,
                    channel_cache={},
                    device_cache={},
                    stack=(),
                )
                run_step_id_by_step_id = {
                    step.id: run_step.id
                    for step, run_step in zip(ordered_steps, run.steps)
                }
                root_with_run_steps = self._attach_run_step_ids(resolved_root, run_step_id_by_step_id)
                resolved_cache[sequence_id] = root_with_run_steps
                await session.commit()
                return run.id, root_with_run_steps, resolved_cache
            except (ChannelNotFoundError, SequenceNotApplicableError, SequenceNotFoundError):
                await session.rollback()
                raise
            except IntegrityError as exc:
                await session.rollback()
                if self._is_active_run_violation(exc):
                    raise SequenceAlreadyRunningError(f"Sequence {sequence.id} already running") from exc
                raise

    async def _build_resolved_sequence(
        self,
        session,
        sequence_id: int,
        *,
        workspace_id: int | None,
        signal_bindings: dict[str, int],
        resolved_cache: dict[int, ResolvedSequenceDefinition],
        channel_cache: dict[int, ChannelInfo],
        device_cache: dict[int, DeviceInfo],
        stack: tuple[int, ...],
    ) -> ResolvedSequenceDefinition:
        cached = resolved_cache.get(sequence_id)
        if cached is not None:
            return cached

        if sequence_id in stack:
            cycle = " -> ".join(map(str, [*stack, sequence_id]))
            raise SequenceNotApplicableError(f"Sequence call cycle detected: {cycle}")

        sequence = await self._load_sequence(session, sequence_id, include_steps=True)
        if not sequence:
            raise SequenceNotFoundError(f"Sequence {sequence_id} not found")

        ordered_steps = sorted(sequence.steps, key=lambda step: step.order_index)
        if not ordered_steps:
            raise SequenceNotApplicableError(f"Sequence {sequence.name} has no steps to execute")

        payload_by_step_id: Dict[int, dict[str, Any]] = {}
        primary_channel_by_step_id: Dict[int, int | None] = {}
        primary_channel_ids: set[int] = set()
        payload_channel_ids: set[int] = set()
        device_ids: set[int] = set()

        switchgear_ids = {
            int(step.payload.get("switchgear_id"))
            for step in ordered_steps
            if step.sequence_step_type == SequenceStepType.DO_PAIR
            and isinstance(step.payload, dict)
            and step.payload.get("switchgear_id") is not None
        }
        switchgear_lookup: dict[int, Switchgear] = {}
        if switchgear_ids:
            if workspace_id is None:
                raise SequenceNotApplicableError("Switchgear pair steps require a workspace context")
            switchgear_result = await session.execute(
                select(Switchgear)
                .join(WorkspaceSwitchgear, WorkspaceSwitchgear.switchgear_id == Switchgear.id)
                .where(
                    Switchgear.id.in_(switchgear_ids),
                    WorkspaceSwitchgear.workspace_id == workspace_id,
                )
                .options(selectinload(Switchgear.bindings))
            )
            switchgear_lookup = {item.id: item for item in switchgear_result.scalars().unique().all()}

        for step in ordered_steps:
            payload = self._resolve_payload_channel_ids(
                step.payload or {},
                signal_bindings=signal_bindings,
            )
            if step.sequence_step_type == SequenceStepType.DO_PAIR and payload.get("switchgear_id") is not None:
                switchgear_id = int(payload["switchgear_id"])
                switchgear = switchgear_lookup.get(switchgear_id)
                if switchgear is None:
                    raise SequenceNotApplicableError(f"Configured switchgear #{switchgear_id} is not available")
                bindings = {binding.role: binding.channel_id for binding in switchgear.bindings}
                open_channel_id = bindings.get("do_open")
                closed_channel_id = bindings.get("do_closed")
                if open_channel_id is None or closed_channel_id is None or open_channel_id == closed_channel_id:
                    raise SequenceNotApplicableError(
                        f"Switchgear {switchgear.name} requires distinct do_open and do_closed channels"
                    )
                payload["channel_ids"] = [int(open_channel_id), int(closed_channel_id)]
            resolved_primary = self._resolve_primary_channel_id(
                step=step,
                payload=payload,
                signal_bindings=signal_bindings,
            )
            target_sequence_id = self._resolve_target_sequence_id(step=step, payload=payload)
            if target_sequence_id is not None:
                payload["target_sequence_id"] = target_sequence_id

            payload_by_step_id[step.id] = payload
            primary_channel_by_step_id[step.id] = resolved_primary

            if resolved_primary is not None:
                primary_channel_ids.add(int(resolved_primary))

            for channel_id in payload.get("channel_ids") or []:
                if channel_id is None:
                    continue
                payload_channel_ids.add(int(channel_id))

            device_id = payload.get("device_id")
            if device_id is not None:
                device_ids.add(int(device_id))

        all_channel_ids = primary_channel_ids | payload_channel_ids
        channel_lookup = await self._load_channel_infos(
            session,
            all_channel_ids,
            channel_cache=channel_cache,
        )
        for channel in channel_lookup.values():
            device_ids.add(channel.device_id)

        device_lookup = await self._load_device_infos(
            session,
            device_ids,
            device_cache=device_cache,
        )

        next_stack = (*stack, sequence_id)
        steps: list[ResolvedSequenceStep] = []
        for step in ordered_steps:
            payload = dict(payload_by_step_id.get(step.id) or {})
            target_sequence_id = payload.get("target_sequence_id")
            if target_sequence_id is not None:
                await self._build_resolved_sequence(
                    session,
                    int(target_sequence_id),
                    workspace_id=workspace_id,
                    signal_bindings=signal_bindings,
                    resolved_cache=resolved_cache,
                    channel_cache=channel_cache,
                    device_cache=device_cache,
                    stack=next_stack,
                )

            primary_channel_id = primary_channel_by_step_id.get(step.id)
            primary = channel_lookup.get(primary_channel_id) if primary_channel_id else None
            pair_channels: list[ChannelInfo] = []
            for channel_id in payload.get("channel_ids") or []:
                info = channel_lookup.get(int(channel_id))
                if info:
                    pair_channels.append(info)

            target_device = None
            payload_device_id = payload.get("device_id")
            if payload_device_id is not None:
                target_device = device_lookup.get(int(payload_device_id))
            elif primary:
                target_device = device_lookup.get(primary.device_id)

            steps.append(
                ResolvedSequenceStep(
                    sequence_id=sequence.id,
                    sequence_name=sequence.name,
                    sequence_step_id=step.id,
                    order_index=step.order_index,
                    total_steps=len(ordered_steps),
                    step_type=step.sequence_step_type,
                    payload=payload,
                    primary_channel=primary,
                    pair_channels=pair_channels,
                    target_device=target_device,
                )
            )

        resolved = ResolvedSequenceDefinition(id=sequence.id, name=sequence.name, steps=steps)
        resolved_cache[sequence_id] = resolved
        return resolved

    async def _load_channel_infos(
        self,
        session,
        channel_ids: set[int],
        *,
        channel_cache: dict[int, ChannelInfo],
    ) -> dict[int, ChannelInfo]:
        if not channel_ids:
            return {}

        missing_ids = sorted(channel_ids - channel_cache.keys())
        if missing_ids:
            channel_stmt = (
                select(Channel)
                .options(selectinload(Channel.device))
                .where(Channel.id.in_(missing_ids))
            )
            channel_rows = await session.execute(channel_stmt)
            channels = {channel.id: channel for channel in channel_rows.scalars()}
            missing_channels = sorted(set(missing_ids) - channels.keys())
            if missing_channels:
                raise ChannelNotFoundError(
                    "Channels not found: " + ", ".join(map(str, missing_channels))
                )
            for channel in channels.values():
                if channel.device is None:
                    raise SequenceNotApplicableError(
                        f"Channel {channel.id} is not attached to a device"
                    )
                channel_cache[channel.id] = ChannelInfo(
                    id=channel.id,
                    device_id=channel.device_id,
                    unit_id=channel.device.unit_id,
                    channel_index=channel.channel_index,
                )

        return {
            channel_id: channel_cache[channel_id]
            for channel_id in channel_ids
            if channel_id in channel_cache
        }

    async def _load_device_infos(
        self,
        session,
        device_ids: set[int],
        *,
        device_cache: dict[int, DeviceInfo],
    ) -> dict[int, DeviceInfo]:
        if not device_ids:
            return {}

        missing_ids = sorted(device_ids - device_cache.keys())
        if missing_ids:
            device_stmt = select(Device).where(Device.id.in_(missing_ids))
            device_rows = await session.execute(device_stmt)
            devices = {device.id: device for device in device_rows.scalars()}
            missing_devices = sorted(set(missing_ids) - devices.keys())
            if missing_devices:
                raise SequenceNotApplicableError(
                    "Devices not found: " + ", ".join(map(str, missing_devices))
                )
            for device in devices.values():
                device_cache[device.id] = DeviceInfo(
                    id=device.id,
                    unit_id=device.unit_id,
                    channel_ids=[int(channel.id) for channel in (device.channels or [])],
                    channel_indexes=[int(channel.channel_index) for channel in (device.channels or [])],
                )

        return {
            device_id: device_cache[device_id]
            for device_id in device_ids
            if device_id in device_cache
        }

    @staticmethod
    def _attach_run_step_ids(
        sequence: ResolvedSequenceDefinition,
        run_step_id_by_step_id: dict[int, int],
    ) -> ResolvedSequenceDefinition:
        return ResolvedSequenceDefinition(
            id=sequence.id,
            name=sequence.name,
            steps=[
                ResolvedSequenceStep(
                    sequence_id=step.sequence_id,
                    sequence_name=step.sequence_name,
                    sequence_step_id=step.sequence_step_id,
                    order_index=step.order_index,
                    total_steps=step.total_steps,
                    step_type=step.step_type,
                    payload=dict(step.payload),
                    primary_channel=step.primary_channel,
                    pair_channels=list(step.pair_channels),
                    target_device=step.target_device,
                    run_step_id=run_step_id_by_step_id.get(step.sequence_step_id),
                )
                for step in sequence.steps
            ],
        )

    @staticmethod
    def _resolve_target_sequence_id(*, step, payload: dict[str, Any]) -> int | None:
        step_type = getattr(step.sequence_step_type, "value", str(step.sequence_step_type))
        if step_type not in {"CALL_SEQUENCE", "REPEAT_SEQUENCE"}:
            return None

        raw_target = payload.get("target_sequence_id")
        if raw_target is None or str(raw_target).strip() == "":
            raise SequenceNotApplicableError(f"{step_type} step requires target_sequence_id")

        try:
            target_sequence_id = int(raw_target)
        except (TypeError, ValueError) as exc:
            raise SequenceNotApplicableError(
                f"{step_type} target_sequence_id must be a positive integer"
            ) from exc

        if target_sequence_id <= 0:
            raise SequenceNotApplicableError(
                f"{step_type} target_sequence_id must be a positive integer"
            )

        return target_sequence_id

    @staticmethod
    def _parse_repeat_config(payload: dict[str, Any]) -> RepeatStepConfig:
        raw_mode = str(payload.get("repeat_mode") or "times").strip().lower()
        if raw_mode == "times":
            raw_iterations = payload.get("iterations")
            try:
                iterations = int(raw_iterations)
            except (TypeError, ValueError) as exc:
                raise SequenceNotApplicableError("REPEAT_SEQUENCE iterations must be a positive integer") from exc
            if iterations <= 0:
                raise SequenceNotApplicableError("REPEAT_SEQUENCE iterations must be a positive integer")
            return RepeatStepConfig(mode="times", iterations=iterations)

        if raw_mode == "duration":
            raw_duration = payload.get("duration_ms")
            try:
                duration_ms = int(raw_duration)
            except (TypeError, ValueError) as exc:
                raise SequenceNotApplicableError("REPEAT_SEQUENCE duration_ms must be a positive integer") from exc
            if duration_ms <= 0:
                raise SequenceNotApplicableError("REPEAT_SEQUENCE duration_ms must be a positive integer")
            return RepeatStepConfig(mode="duration", duration_ms=duration_ms)

        if raw_mode == "until_stopped":
            return RepeatStepConfig(mode="until_stopped")

        raise SequenceNotApplicableError(
            "REPEAT_SEQUENCE repeat_mode must be one of: times, duration, until_stopped"
        )

    @staticmethod
    def _resolve_payload_channel_ids(
        payload_raw: dict,
        *,
        signal_bindings: dict[str, int],
    ) -> dict:
        payload = dict(payload_raw or {})

        raw_channel_ids = payload.get("channel_ids")
        if isinstance(raw_channel_ids, list) and raw_channel_ids:
            payload["channel_ids"] = [int(channel_id) for channel_id in raw_channel_ids if channel_id is not None]
            return payload

        signal_keys = payload.get("signal_keys")
        if not isinstance(signal_keys, list) or not signal_keys:
            return payload

        resolved: list[int] = []
        missing: list[str] = []
        for raw_key in signal_keys:
            key = str(raw_key).strip() if raw_key is not None else ""
            if not key:
                continue
            channel_id = signal_bindings.get(key)
            if channel_id is None:
                missing.append(key)
                continue
            resolved.append(int(channel_id))

        if missing:
            raise SequenceNotApplicableError(
                "Missing channel allocation for signal_keys: " + ", ".join(sorted(set(missing)))
            )
        if resolved:
            payload["channel_ids"] = resolved
        return payload

    @staticmethod
    def _resolve_primary_channel_id(
        *,
        step,
        payload: dict,
        signal_bindings: dict[str, int],
    ) -> int | None:
        if step.channel_id is not None:
            return int(step.channel_id)

        raw_signal_key = payload.get("signal_key")
        signal_key = str(raw_signal_key).strip() if raw_signal_key is not None else ""
        if not signal_key:
            return None

        channel_id = signal_bindings.get(signal_key)
        if channel_id is None and SequenceRunner._step_requires_primary_channel(step):
            raise SequenceNotApplicableError(f"Missing channel allocation for signal_key '{signal_key}'")
        return int(channel_id) if channel_id is not None else None

    @staticmethod
    def _step_requires_primary_channel(step) -> bool:
        step_type = getattr(step.sequence_step_type, "value", str(step.sequence_step_type))
        return step_type in {"DO_LATCH", "DO_PULSE", "AO_SET"}

    async def _execute_run(
        self,
        sequence_id: int,
        run_id: int,
        root_sequence: ResolvedSequenceDefinition,
        resolved_sequences: dict[int, ResolvedSequenceDefinition],
        cancel_event: asyncio.Event,
        request_id: Optional[str],
        requested_by: Optional[str],
        workspace_id: Optional[int],
    ) -> None:
        total_steps = len(root_sequence.steps)
        start_time = time.monotonic()
        started_at = datetime.now(timezone.utc)
        completed_step_ids: List[int] = []
        blocked_step_ids: List[int] = []
        non_terminal_failures: List[str] = []

        try:
            async with AsyncSessionLocal() as session:
                activated = await self._activate_run(session, run_id, started_at)
                if not activated:
                    cancellation_index = await self._handle_activation_skip(
                        session=session,
                        sequence_id=sequence_id,
                        run_id=run_id,
                        total_steps=total_steps,
                        completed_step_ids=completed_step_ids,
                        started_at=started_at,
                        start_time=start_time,
                    )
                    if cancellation_index is None:
                        await session.rollback()
                        return
                    await session.commit()
                    await self._publish_terminal_state(
                        sequence_id=sequence_id,
                        run_id=run_id,
                        status="stopped",
                        current_step_index=cancellation_index,
                        total_steps=total_steps,
                        completed_step_ids=completed_step_ids,
                        started_at=started_at,
                        start_time=start_time,
                    )
                    return

                await session.commit()
                await self._on_run_started(
                    sequence_id=sequence_id,
                    run_id=run_id,
                    total_steps=total_steps,
                    started_at=started_at,
                    request_id=request_id,
                    requested_by=requested_by,
                )

                if workspace_id is None:
                    raise SequenceNotApplicableError("Sequence is not linked to a workspace")
                hardware_admission = HardwareCommandAdmission(RedisManager.get_instance())
                device_presence = DevicePresenceService()
                unavailable_units: set[str] = set()

                async def admit_sequence_command(
                    ctx: StepContext,
                    action: str,
                    channel_id: int | list[int] | None,
                    device_id: int | None,
                    unit_id: str,
                    command_payload: dict[str, Any],
                    sender,
                ) -> Any:
                    if channel_id is None or device_id is None:
                        raise SequenceNotApplicableError("Hardware sequence step requires a single channel")
                    normalized_unit_id = str(unit_id).strip()
                    if normalized_unit_id in unavailable_units:
                        raise SequenceStepBlockedError(
                            f"Device {normalized_unit_id} is unavailable after a previous hardware failure",
                            unit_id=normalized_unit_id,
                        )
                    try:
                        presence = await asyncio.wait_for(
                            device_presence.get_presence(normalized_unit_id),
                            timeout=1.0,
                        )
                    except asyncio.TimeoutError:
                        logger.warning(
                            "Device presence check timed out (run=%s, unit=%s)",
                            run_id,
                            normalized_unit_id,
                        )
                        unavailable_units.add(normalized_unit_id)
                        raise SequenceStepBlockedError(
                            f"Device {normalized_unit_id} presence is unavailable",
                            unit_id=normalized_unit_id,
                        )
                    if not presence.online:
                        unavailable_units.add(normalized_unit_id)
                        raise SequenceStepBlockedError(
                            f"Device {normalized_unit_id} is offline",
                            unit_id=normalized_unit_id,
                        )
                    channel_ids = [channel_id] if isinstance(channel_id, int) else list(channel_id)
                    if not channel_ids or len(set(channel_ids)) != len(channel_ids):
                        raise SequenceNotApplicableError("Hardware sequence step has invalid channel set")
                    blocked_channels = await list_hardware_recovery_required_channels(
                        session,
                        channel_ids=[int(channel_id) for channel_id in channel_ids],
                        action=action,
                    )
                    if blocked_channels:
                        raise SequenceNotApplicableError(
                            "Hardware sequence channel requires physical recovery"
                        )
                    owner_id = f"sequence:{run_id}"
                    leases = await hardware_admission.acquire_many(
                        channel_ids=channel_ids,
                        owner_kind="sequence",
                        owner_id=owner_id,
                    )
                    if leases is None:
                        raise SequenceNotApplicableError("Hardware channel is busy")
                    targets, analog = _sequence_readback_targets(
                        action=action,
                        payload=command_payload,
                    )
                    command_id = uuid4().hex
                    try:
                        await record_hardware_command_intent(
                            session,
                            command_id=command_id,
                            workspace_id=int(workspace_id),
                            job_id=str(run_id),
                            attempt_id=None,
                            owner_kind="sequence",
                            owner_id=owner_id,
                            device_id=device_id,
                            channel_id=channel_ids[0],
                            unit_id=unit_id,
                            action=action,
                            payload={**command_payload, "channel_ids": channel_ids},
                            fencing_epoch=leases[0].fencing_epoch,
                        )
                        await session.commit()
                        is_current = getattr(hardware_admission, "is_current", None)
                        lease_lost = False
                        if is_current is not None:
                            for lease in leases:
                                if not await is_current(lease):
                                    lease_lost = True
                                    break
                        if lease_lost:
                            raise SequenceNotApplicableError("Hardware channel lease lost")
                        try:
                            await sender(command_id)
                        except Exception as exc:
                            await session.rollback()
                            await mark_hardware_command_intent_delivery_failure(
                                session,
                                command_id=command_id,
                                status="unknown",
                            )
                            await session.commit()
                            unavailable_units.add(normalized_unit_id)
                            raise SequenceDeviceUnavailableError(
                                f"Hardware command delivery failed: {exc}",
                                unit_id=normalized_unit_id,
                            ) from exc
                        await mark_hardware_command_intent_queued(session, command_id=command_id)
                        await session.commit()
                        states = await wait_for_hardware_command_acks(
                            session,
                            command_ids=[command_id],
                            timeout_ms=3000,
                            cancel_event=cancel_event,
                            cancellation_probe=lambda: self._probe_cancellation_from_db(run_id, cancel_event),
                        )
                        if states.get("__cancelled__") == "cancelled":
                            raise SequenceCancellationRequested()
                        if states.get(command_id) != "acknowledged":
                            await mark_hardware_command_intent_delivery_failure(
                                session,
                                command_id=command_id,
                                status=str(states.get(command_id, "unknown")),
                            )
                            await session.commit()
                            unavailable_units.add(normalized_unit_id)
                            raise SequenceDeviceUnavailableError(
                                f"Hardware command {states.get(command_id, 'unknown')}",
                                unit_id=normalized_unit_id,
                            )
                        readback_packet_id = await enqueue_request_state(
                            unit_id=unit_id,
                            mode=(
                                State.REQ_SINGLE_FLOAT
                                if analog
                                else State.REQ_SINGLE_BIT
                                if len(targets) == 1
                                else State.REQ_ALL_BIT
                            ),
                            ch=targets[0][0] if analog or len(targets) == 1 else None,
                            correlation_id=f"sequence:{command_id}:readback",
                        )
                        if not await _wait_for_sequence_readback(
                            RedisManager.get_instance(),
                            unit_id=unit_id,
                            targets=targets,
                            analog=analog,
                            packet_id=readback_packet_id,
                            cancel_event=cancel_event,
                            cancellation_probe=lambda: self._probe_cancellation_from_db(run_id, cancel_event),
                        ):
                            await mark_hardware_command_intent_delivery_failure(
                                session,
                                command_id=command_id,
                                status="recovery_required",
                            )
                            await session.commit()
                            unavailable_units.add(normalized_unit_id)
                            raise SequenceDeviceUnavailableError(
                                "Hardware state readback failed",
                                unit_id=normalized_unit_id,
                            )
                        if action == "do_pulse":
                            await asyncio.sleep(max(0, int(command_payload.get("pulse_ms", 0))) / 1000)
                            revert_packet_id = await enqueue_request_state(
                                unit_id=unit_id,
                                mode=State.REQ_SINGLE_BIT,
                                ch=int(command_payload["channel_index"]),
                                correlation_id=f"sequence:{command_id}:pulse-revert",
                            )
                            if not await _wait_for_sequence_readback(
                                RedisManager.get_instance(),
                                unit_id=unit_id,
                                targets=[(int(command_payload["channel_index"]), 0)],
                                analog=False,
                                packet_id=revert_packet_id,
                                cancel_event=cancel_event,
                                cancellation_probe=lambda: self._probe_cancellation_from_db(run_id, cancel_event),
                            ):
                                await mark_hardware_command_intent_delivery_failure(
                                    session,
                                    command_id=command_id,
                                    status="recovery_required",
                                )
                                await session.commit()
                                unavailable_units.add(normalized_unit_id)
                                raise SequenceDeviceUnavailableError(
                                    "Hardware pulse restore readback failed",
                                    unit_id=normalized_unit_id,
                                )
                        await mark_hardware_command_intent_completed(session, command_id=command_id)
                        await session.commit()
                        return command_id
                    finally:
                        for lease in locals().get("leases", []) or []:
                            await hardware_admission.release(lease)

                async def bounded_admit_sequence_command(
                    ctx: StepContext,
                    action: str,
                    channel_id: int | list[int] | None,
                    device_id: int | None,
                    unit_id: str,
                    command_payload: dict[str, Any],
                    sender,
                ) -> Any:
                    try:
                        return await asyncio.wait_for(
                            admit_sequence_command(
                                ctx,
                                action,
                                channel_id,
                                device_id,
                                unit_id,
                                command_payload,
                                sender,
                            ),
                            timeout=SEQUENCE_HARDWARE_STEP_TIMEOUT_MS / 1000,
                        )
                    except asyncio.TimeoutError as exc:
                        normalized_unit_id = str(unit_id).strip()
                        unavailable_units.add(normalized_unit_id)
                        await reconcile_unfinished_hardware_command_intents(
                            session,
                            job_id=str(run_id),
                        )
                        await session.commit()
                        raise SequenceDeviceUnavailableError(
                            f"Hardware step timeout after {SEQUENCE_HARDWARE_STEP_TIMEOUT_MS}ms",
                            unit_id=normalized_unit_id,
                        ) from exc

                for top_step in root_sequence.steps:
                    await self._probe_cancellation_from_db(run_id, cancel_event)
                    if cancel_event.is_set():
                        cancellation_index = await self._record_cancellation(
                            session=session,
                            run_id=run_id,
                            current_step=None,
                            step_started_monotonic=None,
                            fallback_index=len(completed_step_ids),
                        )
                        await session.commit()
                        await self._publish_terminal_state(
                            sequence_id=sequence_id,
                            run_id=run_id,
                            status="stopped",
                            current_step_index=cancellation_index,
                            total_steps=total_steps,
                            completed_step_ids=completed_step_ids,
                            started_at=started_at,
                            start_time=start_time,
                        )
                        return

                    top_step_started_at = datetime.now(timezone.utc)
                    top_step_started_monotonic = time.monotonic()
                    top_runtime_cursor = self._build_runtime_cursor(
                        active_sequence=root_sequence,
                        active_step=top_step,
                        execution_path=(root_sequence.name,),
                    )
                    top_runtime = top_runtime_cursor.to_schema(start_time=start_time)

                    if top_step.run_step_id is not None:
                        await self._set_step_running(session, top_step.run_step_id, top_step_started_at)
                    await session.commit()

                    self._cache_running_state(
                        sequence_id=sequence_id,
                        run_id=run_id,
                        current_step_index=top_step.order_index,
                        total_steps=total_steps,
                        completed_step_ids=completed_step_ids,
                        started_at=started_at,
                        runtime=top_runtime,
                    )
                    await SequenceEventStream.step_started(
                        sequence_id=sequence_id,
                        run_id=run_id,
                        step_index=top_step.order_index,
                        step_id=top_step.sequence_step_id,
                        step_type=top_step.step_type.value,
                    )

                    try:
                        await self._execute_resolved_step(
                            sequence_id=sequence_id,
                            run_id=run_id,
                            total_steps=total_steps,
                            started_at=started_at,
                            start_time=start_time,
                            completed_step_ids=completed_step_ids,
                            cancel_event=cancel_event,
                            top_level_step=top_step,
                            active_sequence=root_sequence,
                            active_step=top_step,
                            execution_path=(root_sequence.name,),
                            resolved_sequences=resolved_sequences,
                            command_admission=bounded_admit_sequence_command,
                        )
                    except SequenceCancellationRequested:
                        cancel_event.set()
                        cancellation_index = await self._record_cancellation(
                            session=session,
                            run_id=run_id,
                            current_step=self._build_step_context(top_step=top_step, active_step=top_step),
                            step_started_monotonic=top_step_started_monotonic,
                            fallback_index=len(completed_step_ids),
                        )
                        await session.commit()
                        await self._publish_terminal_state(
                            sequence_id=sequence_id,
                            run_id=run_id,
                            status="stopped",
                            current_step_index=cancellation_index,
                            total_steps=total_steps,
                            completed_step_ids=completed_step_ids,
                            started_at=started_at,
                            start_time=start_time,
                            runtime=top_runtime_cursor.to_schema(start_time=start_time),
                        )
                        return
                    except SequenceStepBlockedError as exc:
                        if top_step.run_step_id is not None:
                            await self._mark_step_status(
                                session,
                                top_step.run_step_id,
                                SequenceRunStepStatus.BLOCKED,
                                top_step_started_monotonic,
                                str(exc),
                            )
                        blocked_step_ids.append(top_step.sequence_step_id)
                        await session.execute(
                            update(SequenceRun)
                            .where(SequenceRun.id == run_id)
                            .values(current_step_index=top_step.order_index + 1)
                        )
                        await session.commit()
                        await self._publish_step_issue(
                            sequence_id=sequence_id,
                            run_id=run_id,
                            step_index=top_step.order_index,
                            step_id=top_step.sequence_step_id,
                            status="blocked",
                            message=str(exc),
                            runtime=top_runtime_cursor.to_schema(start_time=start_time),
                        )
                        continue
                    except SequenceDeviceUnavailableError as exc:
                        if top_step.run_step_id is not None:
                            await self._mark_step_status(
                                session,
                                top_step.run_step_id,
                                SequenceRunStepStatus.ERROR,
                                top_step_started_monotonic,
                                str(exc),
                            )
                        non_terminal_failures.append(str(exc))
                        await session.execute(
                            update(SequenceRun)
                            .where(SequenceRun.id == run_id)
                            .values(current_step_index=top_step.order_index + 1, error_message=str(exc))
                        )
                        await session.commit()
                        await self._publish_step_issue(
                            sequence_id=sequence_id,
                            run_id=run_id,
                            step_index=top_step.order_index,
                            step_id=top_step.sequence_step_id,
                            status="error",
                            message=str(exc),
                            runtime=top_runtime_cursor.to_schema(start_time=start_time),
                        )
                        continue
                    except SequenceNotApplicableError as exc:
                        failure_runtime = self._state_cache.get(sequence_id)
                        await self._handle_step_failure(
                            session=session,
                            sequence_id=sequence_id,
                            run_id=run_id,
                            total_steps=total_steps,
                            started_at=started_at,
                            start_time=start_time,
                            completed_step_ids=completed_step_ids,
                            top_level_step=top_step,
                            top_step_started_monotonic=top_step_started_monotonic,
                            message=str(exc),
                            runtime=failure_runtime.runtime if failure_runtime else top_runtime_cursor.to_schema(start_time=start_time),
                        )
                        return
                    except Exception as exc:  # noqa: BLE001
                        logger.exception(
                            "Sequence step failed unexpectedly (sequence=%s, run=%s, step=%s)",
                            sequence_id,
                            run_id,
                            top_step.sequence_step_id,
                        )
                        await self._handle_step_failure(
                            session=session,
                            sequence_id=sequence_id,
                            run_id=run_id,
                            total_steps=total_steps,
                            started_at=started_at,
                            start_time=start_time,
                            completed_step_ids=completed_step_ids,
                            top_level_step=top_step,
                            top_step_started_monotonic=top_step_started_monotonic,
                            message=str(exc),
                            runtime=(
                                self._state_cache.get(sequence_id).runtime
                                if self._state_cache.get(sequence_id)
                                else top_runtime_cursor.to_schema(start_time=start_time)
                            ),
                        )
                        return

                    if top_step.run_step_id is not None:
                        await self._mark_step_status(
                            session,
                            top_step.run_step_id,
                            SequenceRunStepStatus.COMPLETED,
                            top_step_started_monotonic,
                        )
                    completed_step_ids.append(top_step.sequence_step_id)
                    await session.execute(
                        update(SequenceRun)
                        .where(SequenceRun.id == run_id)
                        .values(current_step_index=top_step.order_index + 1)
                    )
                    await session.commit()

                    await SequenceEventStream.step_completed(
                        sequence_id=sequence_id,
                        run_id=run_id,
                        step_index=top_step.order_index,
                        step_id=top_step.sequence_step_id,
                        step_type=top_step.step_type.value,
                        progress_scope="step",
                        step_elapsed_ms=int((time.monotonic() - top_step_started_monotonic) * 1000),
                        run_elapsed_ms=int((time.monotonic() - start_time) * 1000),
                        completed_step_ids=list(completed_step_ids),
                        runtime=top_runtime_cursor.to_schema(start_time=start_time),
                    )
                    self._cache_running_state(
                        sequence_id=sequence_id,
                        run_id=run_id,
                        current_step_index=top_step.order_index + 1,
                        total_steps=total_steps,
                        completed_step_ids=completed_step_ids,
                        started_at=started_at,
                        runtime=top_runtime_cursor.to_schema(start_time=start_time),
                    )

                final_status = "completed_with_issues" if blocked_step_ids or non_terminal_failures else "completed"
                final_error = (
                    non_terminal_failures[0]
                    if non_terminal_failures
                    else (
                        f"{len(blocked_step_ids)} step(s) blocked by unavailable device"
                        if blocked_step_ids
                        else None
                    )
                )
                if final_status == "completed_with_issues":
                    await self._mark_run_completed_with_issues(
                        session,
                        run_id,
                        error_message=final_error,
                    )
                else:
                    await self._mark_run_completed(session, run_id)
                await session.commit()
                await self._publish_terminal_state(
                    sequence_id=sequence_id,
                    run_id=run_id,
                    status=final_status,
                    current_step_index=total_steps,
                    total_steps=total_steps,
                    completed_step_ids=completed_step_ids,
                    started_at=started_at,
                    start_time=start_time,
                    last_error=final_error,
                    blocked_step_ids=blocked_step_ids,
                )
        except Exception as exc:  # noqa: BLE001
            logger.exception(
                "Sequence run crashed (sequence=%s, run=%s)",
                sequence_id,
                run_id,
            )
            if run_id is not None:
                try:
                    async with AsyncSessionLocal() as recovery_session:
                        reconciled = await reconcile_unfinished_hardware_command_intents(
                            recovery_session,
                            job_id=str(run_id),
                        )
                        await recovery_session.commit()
                    if reconciled:
                        logger.warning(
                            "⚠️ Reconciled %s unfinished hardware intents after sequence crash run=%s",
                            reconciled,
                            run_id,
                        )
                except Exception:  # noqa: BLE001
                    logger.exception(
                        "💥 Failed to reconcile hardware intents after sequence crash run=%s",
                        run_id,
                    )
            raise

    @staticmethod
    def _build_runtime_cursor(
        *,
        active_sequence: ResolvedSequenceDefinition,
        active_step: ResolvedSequenceStep,
        execution_path: tuple[str, ...],
        loop_state: Optional[ExecutionLoopState] = None,
    ) -> RuntimeCursor:
        return RuntimeCursor(
            active_sequence_id=active_sequence.id,
            active_sequence_name=active_sequence.name,
            active_step_id=active_step.sequence_step_id,
            active_step_index=active_step.order_index,
            active_total_steps=active_step.total_steps,
            active_step_type=active_step.step_type.value,
            execution_path=execution_path,
            loop_state=loop_state,
        )

    @staticmethod
    def _build_step_context(
        *,
        top_step: ResolvedSequenceStep,
        active_step: ResolvedSequenceStep,
    ) -> StepContext:
        return StepContext(
            index=top_step.order_index,
            sequence_step_id=top_step.sequence_step_id,
            run_step_id=top_step.run_step_id,
            step_type=active_step.step_type,
            payload=dict(active_step.payload),
            primary_channel=active_step.primary_channel,
            pair_channels=list(active_step.pair_channels),
            target_device=active_step.target_device,
        )

    async def _execute_resolved_step(
        self,
        *,
        sequence_id: int,
        run_id: int,
        total_steps: int,
        started_at: datetime,
        start_time: float,
        completed_step_ids: List[int],
        cancel_event: asyncio.Event,
        top_level_step: ResolvedSequenceStep,
        active_sequence: ResolvedSequenceDefinition,
        active_step: ResolvedSequenceStep,
        execution_path: tuple[str, ...],
        resolved_sequences: dict[int, ResolvedSequenceDefinition],
        command_admission: HardwareCommandAdmissionFn,
        loop_state: Optional[ExecutionLoopState] = None,
    ) -> None:
        await self._probe_cancellation_from_db(run_id, cancel_event)
        if cancel_event.is_set():
            raise SequenceCancellationRequested()

        runtime_cursor = self._build_runtime_cursor(
            active_sequence=active_sequence,
            active_step=active_step,
            execution_path=execution_path,
            loop_state=loop_state,
        )
        self._cache_running_state(
            sequence_id=sequence_id,
            run_id=run_id,
            current_step_index=top_level_step.order_index,
            total_steps=total_steps,
            completed_step_ids=completed_step_ids,
            started_at=started_at,
            runtime=runtime_cursor.to_schema(start_time=start_time),
        )

        if active_step.step_type == SequenceStepType.CALL_SEQUENCE:
            target_sequence_id = int(active_step.payload["target_sequence_id"])
            child_sequence = resolved_sequences.get(target_sequence_id)
            if child_sequence is None:
                raise SequenceNotApplicableError(f"Target sequence {target_sequence_id} is not available")
            child_path = (*execution_path, child_sequence.name)
            for child_step in child_sequence.steps:
                await self._execute_resolved_step(
                    sequence_id=sequence_id,
                    run_id=run_id,
                    total_steps=total_steps,
                    started_at=started_at,
                    start_time=start_time,
                    completed_step_ids=completed_step_ids,
                    cancel_event=cancel_event,
                    top_level_step=top_level_step,
                    active_sequence=child_sequence,
                    active_step=child_step,
                    execution_path=child_path,
                    resolved_sequences=resolved_sequences,
                    command_admission=command_admission,
                    loop_state=loop_state,
                )
            return

        if active_step.step_type == SequenceStepType.REPEAT_SEQUENCE:
            target_sequence_id = int(active_step.payload["target_sequence_id"])
            child_sequence = resolved_sequences.get(target_sequence_id)
            if child_sequence is None:
                raise SequenceNotApplicableError(f"Target sequence {target_sequence_id} is not available")
            repeat = self._parse_repeat_config(active_step.payload)
            deadline = (
                time.monotonic() + (repeat.duration_ms / 1000)
                if repeat.duration_ms is not None
                else None
            )
            iteration = 0
            child_path = (*execution_path, child_sequence.name)

            while True:
                await self._probe_cancellation_from_db(run_id, cancel_event)
                if cancel_event.is_set():
                    raise SequenceCancellationRequested()

                if repeat.mode == "times" and iteration >= (repeat.iterations or 0):
                    break
                if repeat.mode == "duration" and deadline is not None and time.monotonic() >= deadline:
                    break

                iteration += 1
                child_loop = ExecutionLoopState(
                    current=iteration,
                    total=repeat.iterations if repeat.mode == "times" else None,
                    mode=repeat.mode,
                )
                for child_step in child_sequence.steps:
                    await self._execute_resolved_step(
                        sequence_id=sequence_id,
                        run_id=run_id,
                        total_steps=total_steps,
                        started_at=started_at,
                        start_time=start_time,
                        completed_step_ids=completed_step_ids,
                        cancel_event=cancel_event,
                        top_level_step=top_level_step,
                        active_sequence=child_sequence,
                        active_step=child_step,
                        execution_path=child_path,
                        resolved_sequences=resolved_sequences,
                        command_admission=command_admission,
                        loop_state=child_loop,
                    )

                if repeat.mode == "duration" and deadline is not None and time.monotonic() >= deadline:
                    break
            return

        atomic_started_monotonic = time.monotonic()
        await self._executor.execute_step(
            ctx=self._build_step_context(top_step=top_level_step, active_step=active_step),
            cancel_event=cancel_event,
            cancellation_probe=lambda: self._probe_cancellation_from_db(run_id, cancel_event),
            command_admission=command_admission,
        )

        is_nested_progress = (
            active_sequence.id != top_level_step.sequence_id
            or active_step.sequence_step_id != top_level_step.sequence_step_id
            or loop_state is not None
        )
        if not is_nested_progress:
            return

        await SequenceEventStream.step_completed(
            sequence_id=sequence_id,
            run_id=run_id,
            step_index=top_level_step.order_index,
            step_id=top_level_step.sequence_step_id,
            step_type=top_level_step.step_type.value,
            progress_scope="nested_step",
            step_elapsed_ms=int((time.monotonic() - atomic_started_monotonic) * 1000),
            run_elapsed_ms=int((time.monotonic() - start_time) * 1000),
            completed_step_ids=list(completed_step_ids),
            runtime=runtime_cursor.to_schema(start_time=start_time),
        )

    async def _handle_step_failure(
        self,
        *,
        session,
        sequence_id: int,
        run_id: int,
        total_steps: int,
        started_at: datetime,
        start_time: float,
        completed_step_ids: List[int],
        top_level_step: ResolvedSequenceStep,
        top_step_started_monotonic: float,
        message: str,
        runtime: Optional[SequenceRuntimeSchema],
    ) -> None:
        if top_level_step.run_step_id is not None:
            await self._mark_step_status(
                session,
                top_level_step.run_step_id,
                SequenceRunStepStatus.ERROR,
                top_step_started_monotonic,
                message,
            )
        await self._mark_run_error(session, run_id, top_level_step.order_index, message)
        await session.commit()
        await SequenceEventStream.failed(
            sequence_id=sequence_id,
            run_id=run_id,
            message=message,
            step_index=top_level_step.order_index,
            step_id=top_level_step.sequence_step_id,
            runtime=runtime,
        )
        self._cache_state(
            sequence_id,
            SequenceStateSchema(
                sequence_id=sequence_id,
                status="error",
                run_id=run_id,
                current_step_index=top_level_step.order_index,
                total_steps=total_steps,
                completed_step_ids=list(completed_step_ids),
                last_error=message,
                started_at=started_at,
                finished_at=datetime.now(timezone.utc),
                runtime=runtime,
            ),
        )
        self._reset_cancellation_probe(run_id)

    def _build_executor_hooks(
        self,
        *,
        session,
        sequence_id: int,
        run_id: int,
        total_steps: int,
        started_at: datetime,
        start_time: float,
        request_id: Optional[str],
        requested_by: Optional[str],
    ) -> SequenceExecutorHooks:
        run_start_time = start_time
        completed_cache: List[int] = []

        async def on_run_started(event: RunStartedEvent) -> None:
            nonlocal run_start_time
            run_start_time = event.start_time
            await self._on_run_started(
                sequence_id=sequence_id,
                run_id=run_id,
                total_steps=event.total_steps,
                started_at=started_at,
                request_id=request_id,
                requested_by=requested_by,
            )

        async def on_step_started(event: StepLifecycleEvent) -> None:
            ctx = event.context
            if ctx.run_step_id is not None:
                await self._set_step_running(session, ctx.run_step_id, event.started_at)
                await session.commit()
            await SequenceEventStream.step_started(
                sequence_id=sequence_id,
                run_id=run_id,
                step_index=ctx.index,
                step_id=ctx.sequence_step_id,
                step_type=ctx.step_type.value,
            )

        async def on_step_completed(event: StepCompletedEvent) -> None:
            nonlocal completed_cache
            ctx = event.context
            if ctx.run_step_id is not None:
                await self._mark_step_status(
                    session,
                    ctx.run_step_id,
                    SequenceRunStepStatus.COMPLETED,
                    event.started_monotonic,
                )
            completed_cache = list(event.completed_step_ids)
            await session.execute(
                update(SequenceRun)
                .where(SequenceRun.id == run_id)
                .values(current_step_index=ctx.index + 1)
            )
            await session.commit()
            await SequenceEventStream.step_completed(
                sequence_id=sequence_id,
                run_id=run_id,
                step_index=ctx.index,
                step_id=ctx.sequence_step_id,
                step_type=ctx.step_type.value,
                progress_scope="step",
                step_elapsed_ms=event.step_elapsed_ms,
                run_elapsed_ms=event.run_elapsed_ms,
                completed_step_ids=completed_cache,
            )
            self._cache_state(
                sequence_id,
                SequenceStateSchema(
                    sequence_id=sequence_id,
                    status="running",
                    run_id=run_id,
                    current_step_index=ctx.index + 1,
                    total_steps=total_steps,
                    completed_step_ids=list(completed_cache),
                    last_error=None,
                    started_at=started_at,
                    finished_at=None,
                ),
            )

        async def on_step_failed(event: StepFailedEvent) -> None:
            ctx = event.context
            if event.exception is not None:
                logger.exception(
                    "Sequence step failed (sequence=%s, run=%s, step=%s)",
                    sequence_id,
                    run_id,
                    ctx.sequence_step_id,
                    exc_info=event.exception,
                )
            else:
                logger.warning(
                    "Sequence step invalid (sequence=%s, run=%s, step=%s): %s",
                    sequence_id,
                    run_id,
                    ctx.sequence_step_id,
                    event.message,
                )
            if ctx.run_step_id is not None:
                await self._mark_step_status(
                    session,
                    ctx.run_step_id,
                    SequenceRunStepStatus.ERROR,
                    event.started_monotonic,
                    event.message,
                )
            await self._mark_run_error(session, run_id, ctx.index, event.message)
            await session.commit()
            await SequenceEventStream.failed(
                sequence_id=sequence_id,
                run_id=run_id,
                message=event.message,
                step_index=ctx.index,
                step_id=ctx.sequence_step_id,
            )
            self._cache_state(
                sequence_id,
                SequenceStateSchema(
                    sequence_id=sequence_id,
                    status="error",
                    run_id=run_id,
                    current_step_index=ctx.index,
                    total_steps=total_steps,
                    completed_step_ids=list(completed_cache),
                    last_error=event.message,
                    started_at=started_at,
                    finished_at=datetime.now(timezone.utc),
                ),
            )
            self._reset_cancellation_probe(run_id)

        async def on_cancellation(event: CancellationEvent) -> int:
            index = await self._record_cancellation(
                session=session,
                run_id=run_id,
                current_step=event.current_step,
                step_started_monotonic=event.started_monotonic,
                fallback_index=event.fallback_index,
            )
            await session.commit()
            return index

        async def on_finished(result: SequenceExecutionResult) -> None:
            if result.status == "completed":
                await self._mark_run_completed(session, run_id)
                await session.commit()
            await self._publish_terminal_state(
                sequence_id=sequence_id,
                run_id=run_id,
                status=result.status,
                current_step_index=result.current_step_index,
                total_steps=total_steps,
                completed_step_ids=list(result.completed_step_ids),
                started_at=started_at,
                start_time=run_start_time,
                last_error=result.last_error,
            )

        return SequenceExecutorHooks(
            on_run_started=on_run_started,
            on_step_started=on_step_started,
            on_step_completed=on_step_completed,
            on_step_failed=on_step_failed,
            on_cancellation=on_cancellation,
            on_finished=on_finished,
        )

    async def _mark_run_completed(self, session, run_id: int) -> None:
        finished_at = datetime.now(timezone.utc)
        await session.execute(
            update(SequenceRun)
            .where(SequenceRun.id == run_id)
            .values(
                status=SequenceRunStatus.COMPLETED,
                finished_at=finished_at,
                error_message=None,
            )
        )

    async def _mark_run_completed_with_issues(
        self,
        session,
        run_id: int,
        *,
        error_message: Optional[str],
    ) -> None:
        finished_at = datetime.now(timezone.utc)
        await session.execute(
            update(SequenceRun)
            .where(SequenceRun.id == run_id)
            .values(
                status=SequenceRunStatus.COMPLETED_WITH_ISSUES,
                finished_at=finished_at,
                error_message=error_message,
            )
        )

    async def _transition_to_cancelling(self, sequence_id: int, run_id: int) -> None:
        async with AsyncSessionLocal() as session:
            stmt = (
                update(SequenceRun)
                .where(
                    SequenceRun.id == run_id,
                    SequenceRun.status.in_(
                        [SequenceRunStatus.RUNNING, SequenceRunStatus.PENDING]
                    ),
                )
                .values(status=SequenceRunStatus.CANCELLING)
            )
            result = await session.execute(stmt)
            updated = result.rowcount > 0
            await session.commit()

        if not updated:
            logger.debug("Run %s already not running while requesting stop", run_id)
            return

        self.invalidate_state(sequence_id)
        state = await self.get_state(sequence_id)
        self._cache_state(sequence_id, state)
        if state.run_id != run_id:
            logger.debug(
                "Skipping stopping event for sequence %s run %s (state run %s)",
                sequence_id,
                run_id,
                state.run_id,
            )
            return
        await self._ensure_stopping_event(
            sequence_id=sequence_id,
            run_id=run_id,
            current_step_index=state.current_step_index,
            total_steps=state.total_steps,
        )

    async def _mark_run_error(
        self,
        session,
        run_id: int,
        step_index: int,
        message: str,
    ) -> None:
        finished_at = datetime.now(timezone.utc)
        await session.execute(
            update(SequenceRun)
            .where(SequenceRun.id == run_id)
            .values(
                status=SequenceRunStatus.ERROR,
                finished_at=finished_at,
                current_step_index=step_index,
                error_message=message,
            )
        )

    async def _mark_run_stopped(self, session, run_id: int, step_index: int) -> None:
        finished_at = datetime.now(timezone.utc)
        await session.execute(
            update(SequenceRun)
            .where(SequenceRun.id == run_id)
            .values(
                status=SequenceRunStatus.STOPPED,
                finished_at=finished_at,
                current_step_index=step_index,
            )
        )
        logger.info("Sequence run %s stopped at step %s", run_id, step_index)

    async def _mark_step_status(
        self,
        session,
        run_step_id: int,
        status: SequenceRunStepStatus,
        started_monotonic: float,
        error_message: Optional[str] = None,
    ) -> None:
        finished_at = datetime.now(timezone.utc)
        elapsed_ms = int((time.monotonic() - started_monotonic) * 1000)
        await session.execute(
            update(SequenceRunStep)
            .where(SequenceRunStep.id == run_step_id)
            .values(
                status=status,
                finished_at=finished_at,
                error_message=error_message,
                elapsed_ms=elapsed_ms,
            )
        )

    async def _load_sequence(self, session, sequence_id: int, include_steps: bool = False) -> Optional[Sequence]:
        query = select(Sequence)
        if include_steps:
            query = query.options(
                selectinload(Sequence.steps)
                .selectinload(SequenceStep.channel)
                .selectinload(Channel.device)
            )
        query = query.where(Sequence.id == sequence_id)
        result = await session.execute(query)
        return result.scalar_one_or_none()

    def _cache_state(self, sequence_id: int, state: SequenceStateSchema) -> None:
        self._state_cache[sequence_id] = state

    def invalidate_state(self, sequence_id: int) -> None:
        self._state_cache.pop(sequence_id, None)

    @staticmethod
    def _is_active_run_violation(exc: IntegrityError) -> bool:
        constraint = getattr(getattr(getattr(exc, "orig", None), "diag", None), "constraint_name", None)
        if constraint and constraint == "uq_sequence_runs_active":
            return True
        return "uq_sequence_runs_active" in str(exc).lower()
