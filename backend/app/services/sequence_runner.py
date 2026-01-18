from __future__ import annotations

import asyncio
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Dict, List, Optional, Set

from sqlalchemy import select, update
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import selectinload

from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.models.channel import Channel
from app.models.device import Device
from app.models.sequence import Sequence, SequenceStep
from app.models.sequence_run import (
    SequenceRun,
    SequenceRunStatus,
    SequenceRunStep,
    SequenceRunStepStatus,
)
from app.schemas.sequence_run_schema import SequenceStateSchema
from app.services.domain_errors import (
    ChannelNotFoundError,
    SequenceNotApplicableError,
)
from app.services.sequence_executor import (
    CancellationEvent,
    ChannelInfo,
    DeviceInfo,
    RunStartedEvent,
    SequenceExecutionResult,
    SequenceExecutor,
    SequenceExecutorHooks,
    StepCompletedEvent,
    StepContext,
    StepFailedEvent,
    StepLifecycleEvent,
)
from app.services.sequence_event_stream import SequenceEventStream


logger = get_logger("sequence.runner")


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
        request_id: Optional[str] = None,
        requested_by: Optional[str] = None,
    ) -> SequenceStateSchema:
        self.invalidate_state(sequence_id)

        async with self._lock:
            if sequence_id in self._active_runs:
                raise SequenceAlreadyRunningError(f"Sequence {sequence_id} already running")

            run_id, contexts = await self._create_run(sequence_id)
            cancel_event = asyncio.Event()
            task = asyncio.create_task(
                self._execute_run(
                    sequence_id=sequence_id,
                    run_id=run_id,
                    contexts=contexts,
                    cancel_event=cancel_event,
                    request_id=request_id,
                    requested_by=requested_by,
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

        return await self.get_state(sequence_id)

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
                last_error=last_error,
                started_at=started_at,
                finished_at=finished_at,
            ),
        )
        self._reset_cancellation_probe(run_id)
        self._clear_stopping_event_flag(run_id)
        if status == "completed":
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
    ) -> None:
        await SequenceEventStream.started(
            sequence_id=sequence_id,
            run_id=run_id,
            total_steps=total_steps,
            request_id=request_id,
            requested_by=requested_by,
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
            ),
        )
        logger.info(
            "Sequence %s run %s started (%s steps)",
            sequence_id,
            run_id,
            total_steps,
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

    async def _create_run(self, sequence_id: int) -> tuple[int, List[StepContext]]:
        async with AsyncSessionLocal() as session:
            sequence = await self._load_sequence(session, sequence_id, include_steps=True)
            if not sequence:
                raise SequenceNotFoundError(f"Sequence {sequence_id} not found")

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

                primary_channel_ids: set[int] = {
                    step.channel_id for step in ordered_steps if step.channel_id
                }
                payload_channel_ids: set[int] = set()
                device_ids: set[int] = set()

                for step in ordered_steps:
                    payload = step.payload or {}
                    signal_key = payload.get("signal_key")
                    signal_keys = payload.get("signal_keys")
                    if signal_key or signal_keys:
                        raise SequenceNotApplicableError(
                            "Signal references require allocation metadata and are not supported yet"
                        )
                    for channel_id in payload.get("channel_ids") or []:
                        if channel_id is None:
                            continue
                        payload_channel_ids.add(int(channel_id))
                    device_id = payload.get("device_id")
                    if device_id is not None:
                        device_ids.add(int(device_id))

                all_channel_ids = primary_channel_ids | payload_channel_ids
                channel_lookup: Dict[int, ChannelInfo] = {}
                if all_channel_ids:
                    channel_stmt = (
                        select(Channel)
                        .options(selectinload(Channel.device))
                        .where(Channel.id.in_(all_channel_ids))
                    )
                    channel_rows = await session.execute(channel_stmt)
                    channels = {channel.id: channel for channel in channel_rows.scalars()}
                    missing_channels = sorted(all_channel_ids - channels.keys())
                    if missing_channels:
                        raise ChannelNotFoundError(
                            "Channels not found: " + ", ".join(map(str, missing_channels))
                        )
                    for channel in channels.values():
                        if channel.device is None:
                            raise SequenceNotApplicableError(
                                f"Channel {channel.id} is not attached to a device"
                            )
                        channel_lookup[channel.id] = ChannelInfo(
                            id=channel.id,
                            device_id=channel.device_id,
                            unit_id=channel.device.unit_id,
                            channel_index=channel.channel_index,
                        )
                        device_ids.add(channel.device_id)

                device_lookup: Dict[int, DeviceInfo] = {}
                if device_ids:
                    device_stmt = select(Device).where(Device.id.in_(device_ids))
                    device_rows = await session.execute(device_stmt)
                    devices = {device.id: device for device in device_rows.scalars()}
                    missing_devices = sorted(device_ids - devices.keys())
                    if missing_devices:
                        raise SequenceNotApplicableError(
                            "Devices not found: " + ", ".join(map(str, missing_devices))
                        )
                    for device in devices.values():
                        device_lookup[device.id] = DeviceInfo(id=device.id, unit_id=device.unit_id)

                contexts: List[StepContext] = []
                for index, (step, run_step) in enumerate(zip(ordered_steps, run.steps)):
                    payload = dict(step.payload or {})
                    primary = channel_lookup.get(step.channel_id) if step.channel_id else None
                    pair_channels = []
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

                    contexts.append(
                        StepContext(
                            index=index,
                            sequence_step_id=step.id,
                            run_step_id=run_step.id,
                            step_type=step.sequence_step_type,
                            payload=payload,
                            primary_channel=primary,
                            pair_channels=pair_channels,
                            target_device=target_device,
                        )
                    )

                await session.commit()
                return run.id, contexts
            except (ChannelNotFoundError, SequenceNotApplicableError):
                await session.rollback()
                raise
            except IntegrityError as exc:
                await session.rollback()
                if self._is_active_run_violation(exc):
                    raise SequenceAlreadyRunningError(f"Sequence {sequence.id} already running") from exc
                raise

    async def _execute_run(
        self,
        sequence_id: int,
        run_id: int,
        contexts: List[StepContext],
        cancel_event: asyncio.Event,
        request_id: Optional[str],
        requested_by: Optional[str],
    ) -> None:
        total_steps = len(contexts)
        start_time = time.monotonic()
        started_at = datetime.now(timezone.utc)
        completed_step_ids: List[int] = []

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

                hooks = self._build_executor_hooks(
                    session=session,
                    sequence_id=sequence_id,
                    run_id=run_id,
                    total_steps=total_steps,
                    started_at=started_at,
                    start_time=start_time,
                    request_id=request_id,
                    requested_by=requested_by,
                )

                await self._executor.run(
                    contexts=contexts,
                    cancel_event=cancel_event,
                    hooks=hooks,
                    cancellation_probe=lambda: self._probe_cancellation_from_db(run_id, cancel_event),
                )
        except Exception as exc:  # noqa: BLE001
            logger.exception(
                "Sequence run crashed (sequence=%s, run=%s)",
                sequence_id,
                run_id,
            )
            raise

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
