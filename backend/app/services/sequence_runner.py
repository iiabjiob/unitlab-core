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
from app.infrastructure.protocol.modes import Cmd
from app.models.channel import Channel
from app.models.device import Device
from app.models.sequence import Sequence, SequenceStep, SequenceStepType
from app.models.sequence_run import (
    SequenceRun,
    SequenceRunStatus,
    SequenceRunStep,
    SequenceRunStepStatus,
)
from app.schemas.sequence_run_schema import SequenceStateSchema
from app.services.command_queue_service import enqueue_ao_command, enqueue_do_command
from app.services.sequence_event_stream import SequenceEventStream


logger = get_logger("sequence.runner")


class SequenceRunnerError(Exception):
    """Base class for sequence runner exceptions."""


class SequenceNotFoundError(SequenceRunnerError):
    """Raised when a target sequence cannot be located."""


class SequenceAlreadyRunningError(SequenceRunnerError):
    """Raised when attempting to start a sequence that is already active."""


class SequenceCancellationRequested(Exception):
    """Internal marker used to unwind execution when cancellation is requested."""


@dataclass(frozen=True)
class ChannelInfo:
    id: int
    device_id: int
    unit_id: str
    channel_index: int


@dataclass(frozen=True)
class DeviceInfo:
    id: int
    unit_id: str


@dataclass(frozen=True)
class StepContext:
    index: int
    sequence_step_id: int
    run_step_id: int
    step_type: SequenceStepType
    payload: Dict[str, Any]
    primary_channel: Optional[ChannelInfo]
    pair_channels: List[ChannelInfo]
    target_device: Optional[DeviceInfo]


@dataclass
class ActiveRun:
    sequence_id: int
    run_id: int
    cancel_event: asyncio.Event
    task: asyncio.Task[None]


class SequenceRunner:
    """Coordinates the lifecycle of sequence runs for the worker."""

    _CANCELLATION_PROBE_MIN_INTERVAL = 0.3  # seconds

    def __init__(self) -> None:
        self._active_runs: Dict[int, ActiveRun] = {}
        self._state_cache: Dict[int, SequenceStateSchema] = {}
        self._last_cancellation_probe_at: Dict[int, float] = {}
        self._lock = asyncio.Lock()
        self._stopping_emitted_runs: Set[int] = set()

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
                    sequence_id,
                    run_id,
                    contexts,
                    cancel_event,
                    request_id,
                    requested_by,
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

    async def _run_loop(
        self,
        *,
        session,
        sequence_id: int,
        run_id: int,
        contexts: List[StepContext],
        cancel_event: asyncio.Event,
        completed_step_ids: List[int],
        total_steps: int,
        started_at: datetime,
        start_time: float,
    ) -> None:
        for ctx in contexts:
            await self._probe_cancellation_from_db(run_id, cancel_event)
            if cancel_event.is_set():
                cancellation_index = await self._record_cancellation(
                    session=session,
                    run_id=run_id,
                    current_step=ctx,
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

            should_continue = await self._run_step(
                session=session,
                sequence_id=sequence_id,
                run_id=run_id,
                ctx=ctx,
                cancel_event=cancel_event,
                completed_step_ids=completed_step_ids,
                total_steps=total_steps,
                started_at=started_at,
                start_time=start_time,
            )
            if not should_continue:
                return

        await self._mark_run_completed(session, run_id)
        await session.commit()
        await self._publish_terminal_state(
            sequence_id=sequence_id,
            run_id=run_id,
            status="completed",
            current_step_index=total_steps,
            total_steps=total_steps,
            completed_step_ids=completed_step_ids,
            started_at=started_at,
            start_time=start_time,
        )

    async def _run_step(
        self,
        *,
        session,
        sequence_id: int,
        run_id: int,
        ctx: StepContext,
        cancel_event: asyncio.Event,
        completed_step_ids: List[int],
        total_steps: int,
        started_at: datetime,
        start_time: float,
    ) -> bool:
        step_started_monotonic = time.monotonic()
        step_started_at = datetime.now(timezone.utc)
        if cancel_event.is_set():
            cancellation_index = await self._record_cancellation(
                session=session,
                run_id=run_id,
                current_step=ctx,
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
            return False

        await self._set_step_running(session, ctx.run_step_id, step_started_at)
        await session.commit()
        await SequenceEventStream.step_started(
            sequence_id=sequence_id,
            run_id=run_id,
            step_index=ctx.index,
            step_id=ctx.sequence_step_id,
            step_type=ctx.step_type.value,
        )

        async def cancellation_probe() -> None:
            await self._probe_cancellation_from_db(run_id, cancel_event)

        try:
            await self._execute_step(ctx, cancel_event, run_id, cancellation_probe)
        except SequenceCancellationRequested:
            logger.info("Sequence %s run %s cancelled", sequence_id, run_id)
            cancel_event.set()
            cancellation_index = await self._record_cancellation(
                session=session,
                run_id=run_id,
                current_step=ctx,
                step_started_monotonic=step_started_monotonic,
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
            return False
        except Exception as exc:  # noqa: BLE001
            message = str(exc)
            logger.exception(
                "Sequence step failed (sequence=%s, run=%s, step=%s)",
                sequence_id,
                run_id,
                ctx.sequence_step_id,
            )
            await self._mark_step_status(
                session,
                ctx.run_step_id,
                SequenceRunStepStatus.ERROR,
                step_started_monotonic,
                message,
            )
            await self._mark_run_error(session, run_id, ctx.index, message)
            await session.commit()
            await SequenceEventStream.failed(
                sequence_id=sequence_id,
                run_id=run_id,
                message=message,
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
                    completed_step_ids=list(completed_step_ids),
                    last_error=message,
                    started_at=started_at,
                    finished_at=datetime.now(timezone.utc),
                ),
            )
            self._reset_cancellation_probe(run_id)
            return False

        await self._mark_step_status(
            session,
            ctx.run_step_id,
            SequenceRunStepStatus.COMPLETED,
            step_started_monotonic,
        )
        completed_step_ids.append(ctx.sequence_step_id)
        await session.execute(
            update(SequenceRun)
            .where(SequenceRun.id == run_id)
            .values(current_step_index=ctx.index + 1)
        )
        await session.commit()

        now_monotonic = time.monotonic()
        step_elapsed_ms = int((now_monotonic - step_started_monotonic) * 1000)
        run_elapsed_ms = int((now_monotonic - start_time) * 1000)
        completed_snapshot = list(completed_step_ids)
        await SequenceEventStream.step_completed(
            sequence_id=sequence_id,
            run_id=run_id,
            step_index=ctx.index,
            step_id=ctx.sequence_step_id,
            step_type=ctx.step_type.value,
            step_elapsed_ms=step_elapsed_ms,
            run_elapsed_ms=run_elapsed_ms,
            completed_step_ids=completed_snapshot,
        )
        self._cache_state(
            sequence_id,
            SequenceStateSchema(
                sequence_id=sequence_id,
                status="running",
                run_id=run_id,
                current_step_index=ctx.index + 1,
                total_steps=total_steps,
                completed_step_ids=completed_snapshot,
                last_error=None,
                started_at=started_at,
                finished_at=None,
            ),
        )
        return True

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
            run = SequenceRun(
                sequence_id=sequence_id,
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
                channel_ids = {step.channel_id for step in ordered_steps if step.channel_id}
                payload_channel_ids: set[int] = set()
                device_ids: set[int] = set()

                for step in ordered_steps:
                    payload = step.payload or {}
                    for cid in payload.get("channel_ids", []) or []:
                        if cid:
                            payload_channel_ids.add(cid)
                    device_id = payload.get("device_id")
                    if device_id:
                        device_ids.add(device_id)

                channel_ids.update(payload_channel_ids)

                channel_lookup: Dict[int, ChannelInfo] = {}
                if channel_ids:
                    channel_stmt = (
                        select(Channel)
                        .options(selectinload(Channel.device))
                        .where(Channel.id.in_(channel_ids))
                    )
                    channel_rows = await session.execute(channel_stmt)
                    for channel in channel_rows.scalars():
                        if channel.device is None:
                            continue
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
                    devices = await session.execute(device_stmt)
                    for device in devices.scalars():
                        device_lookup[device.id] = DeviceInfo(id=device.id, unit_id=device.unit_id)

                contexts: List[StepContext] = []
                for index, (step, run_step) in enumerate(zip(ordered_steps, run.steps)):
                    payload = dict(step.payload or {})
                    primary = channel_lookup.get(step.channel_id) if step.channel_id else None
                    pair_channels = [
                        channel_lookup[cid]
                        for cid in payload.get("channel_ids", []) or []
                        if cid in channel_lookup
                    ]
                    target_device = None
                    payload_device_id = payload.get("device_id")
                    if payload_device_id and payload_device_id in device_lookup:
                        target_device = device_lookup[payload_device_id]
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
            except IntegrityError as exc:
                await session.rollback()
                if self._is_active_run_violation(exc):
                    raise SequenceAlreadyRunningError(f"Sequence {sequence_id} already running") from exc
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

            await self._run_loop(
                session=session,
                sequence_id=sequence_id,
                run_id=run_id,
                contexts=contexts,
                cancel_event=cancel_event,
                completed_step_ids=completed_step_ids,
                total_steps=total_steps,
                started_at=started_at,
                start_time=start_time,
            )

    async def _execute_step(
        self,
        ctx: StepContext,
        cancel_event: asyncio.Event,
        run_id: int,
        cancellation_probe: Callable[[], Awaitable[None]],
    ) -> None:
        step_type = ctx.step_type
        payload = ctx.payload or {}

        if step_type == SequenceStepType.WAIT:
            delay_ms = int(payload.get("ms", 0))
            if delay_ms > 0:
                await self._wait_with_cancellation(cancel_event, delay_ms, cancellation_probe)
            return

        if step_type == SequenceStepType.AO_SET:
            if not ctx.primary_channel:
                raise ValueError("AO_SET step requires a primary channel")
            value = float(payload.get("value", 0))
            await enqueue_ao_command(
                unit_id=ctx.primary_channel.unit_id,
                ch=ctx.primary_channel.channel_index,
                value=value,
            )
            return

        if step_type == SequenceStepType.DO_LATCH:
            if not ctx.primary_channel:
                raise ValueError("DO_LATCH step requires a primary channel")
            value = int(payload.get("value", 0))
            await enqueue_do_command(
                unit_id=ctx.primary_channel.unit_id,
                mode=Cmd.SET_SINGLE_BIT,
                ch=ctx.primary_channel.channel_index,
                value=value,
            )
            return

        if step_type == SequenceStepType.DO_PULSE:
            if not ctx.primary_channel:
                raise ValueError("DO_PULSE step requires a primary channel")
            value = int(payload.get("value", 0))
            pulse_ms = int(payload.get("pulse_ms", 0))
            await enqueue_do_command(
                unit_id=ctx.primary_channel.unit_id,
                mode=Cmd.SET_PULSE_BIT,
                ch=ctx.primary_channel.channel_index,
                value=value,
                pulse_ms=pulse_ms,
            )
            return

        if step_type == SequenceStepType.DO_PAIR:
            if len(ctx.pair_channels) != 2:
                raise ValueError("DO_PAIR step requires two channels")
            first, second = ctx.pair_channels
            if first.unit_id != second.unit_id:
                raise ValueError("Pair channels must belong to the same device")
            state2b = int(payload.get("state2b", 0))
            await enqueue_do_command(
                unit_id=first.unit_id,
                mode=Cmd.SET_PAIR_BIT,
                chA=first.channel_index,
                chB=second.channel_index,
                state2b=state2b,
            )
            return

        if step_type == SequenceStepType.DO_BITMASK:
            if not ctx.target_device:
                raise ValueError("DO_BITMASK step requires device context")
            bitmask = int(payload.get("bitmask", 0))
            await enqueue_do_command(
                unit_id=ctx.target_device.unit_id,
                mode=Cmd.SET_ALL_BIT,
                bitmask=bitmask,
            )
            return

        raise ValueError(f"Unsupported step type {step_type}")

    async def _wait_with_cancellation(
        self,
        cancel_event: asyncio.Event,
        delay_ms: int,
        cancellation_probe: Callable[[], Awaitable[None]],
    ) -> None:
        if delay_ms <= 0:
            return

        remaining = delay_ms / 1000
        while remaining > 0:
            if cancel_event.is_set():
                raise SequenceCancellationRequested()
            await cancellation_probe()
            chunk = min(0.25, remaining)
            try:
                await asyncio.wait_for(cancel_event.wait(), timeout=chunk)
                raise SequenceCancellationRequested()
            except asyncio.TimeoutError:
                remaining -= chunk
        if cancel_event.is_set():
            raise SequenceCancellationRequested()

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

    async def _handle_cancellation(
        self,
        session,
        sequence_id: int,
        run_id: int,
        current_step_index: int,
        total_steps: int,
        completed_step_ids: List[int],
        started_at: datetime,
        start_time: float,
    ) -> None:
        await self._mark_run_stopped(session, run_id, current_step_index)
        elapsed_total = int((time.monotonic() - start_time) * 1000)
        await SequenceEventStream.finished(
            sequence_id=sequence_id,
            run_id=run_id,
            status="stopped",
            elapsed_ms=elapsed_total,
            current_step_index=current_step_index,
            total_steps=total_steps,
        )
        self._cache_state(
            sequence_id,
            SequenceStateSchema(
                sequence_id=sequence_id,
                status="stopped",
                run_id=run_id,
                current_step_index=current_step_index,
                total_steps=total_steps,
                completed_step_ids=list(completed_step_ids),
                last_error=None,
                started_at=started_at,
                finished_at=datetime.now(timezone.utc),
            ),
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
