from __future__ import annotations

import asyncio
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

from sqlalchemy import select, update
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
from app.schemas.ws.events import (
    SequenceCompletedEvent,
    SequenceErrorEvent,
    SequenceProgressEvent,
    SequenceStartedEvent,
    SequenceStepErrorEvent,
    SequenceStoppedEvent,
)
from app.services.command_queue_service import enqueue_ao_command, enqueue_do_command
from app.ws.manager import WebSocketManager

logger = get_logger("sequence")


class SequenceRunnerError(Exception):
    """Base class for runner errors."""


class SequenceNotFoundError(SequenceRunnerError):
    """Raised when target sequence is missing."""


class SequenceAlreadyRunningError(SequenceRunnerError):
    """Raised when attempting to start a sequence that is already running."""


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
    payload: Dict
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
    """Executes sequences on the backend and broadcasts progress via websockets."""

    _instance: Optional["SequenceRunner"] = None

    def __init__(self) -> None:
        self._active_runs: Dict[int, ActiveRun] = {}
        self._state_cache: Dict[int, SequenceStateSchema] = {}
        self._lock = asyncio.Lock()

    @classmethod
    def get_instance(cls) -> "SequenceRunner":
        if cls._instance is None:
            cls._instance = SequenceRunner()
        return cls._instance

    @classmethod
    def reset_instance(cls) -> None:
        cls._instance = None

    async def start(self, sequence_id: int) -> SequenceStateSchema:
        async with self._lock:
            if sequence_id in self._active_runs:
                raise SequenceAlreadyRunningError(f"Sequence {sequence_id} already running")

            run_id, contexts = await self._create_run(sequence_id)
            cancel_event = asyncio.Event()
            task = asyncio.create_task(
                self._execute_run(sequence_id, run_id, contexts, cancel_event),
                name=f"sequence-run-{run_id}",
            )
            self._active_runs[sequence_id] = ActiveRun(
                sequence_id=sequence_id,
                run_id=run_id,
                cancel_event=cancel_event,
                task=task,
            )
            task.add_done_callback(lambda _: self._active_runs.pop(sequence_id, None))

        await self._broadcast_started(sequence_id, run_id, len(contexts))
        logger.info("Sequence %s run %s started (%s steps)", sequence_id, run_id, len(contexts))
        return await self.get_state(sequence_id)

    async def stop(self, sequence_id: int) -> SequenceStateSchema:
        handle: Optional[ActiveRun] = None
        async with self._lock:
            handle = self._active_runs.get(sequence_id)
            if handle:
                handle.cancel_event.set()

        if handle:
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

    async def _create_run(self, sequence_id: int) -> tuple[int, List[StepContext]]:
        async with AsyncSessionLocal() as session:
            sequence = await self._load_sequence(session, sequence_id, include_steps=True)
            if not sequence:
                raise SequenceNotFoundError(f"Sequence {sequence_id} not found")

            ordered_steps = sorted(sequence.steps, key=lambda s: s.order_index)
            run = SequenceRun(
                sequence_id=sequence_id,
                status=SequenceRunStatus.RUNNING,
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

    async def _execute_run(
        self,
        sequence_id: int,
        run_id: int,
        contexts: List[StepContext],
        cancel_event: asyncio.Event,
    ) -> None:
        ws_manager = WebSocketManager.get_instance()
        start_time = time.monotonic()
        started_at = datetime.now(timezone.utc)
        completed_step_ids: List[int] = []
        total_steps = len(contexts)

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

        async with AsyncSessionLocal() as session:
            if not contexts:
                await self._mark_run_completed(session, run_id, start_time)
                elapsed_total = int((time.monotonic() - start_time) * 1000)
                await ws_manager.broadcast(
                    SequenceCompletedEvent(sequence_id=sequence_id, run_id=run_id, elapsed_ms=elapsed_total)
                )
                self._cache_state(
                    sequence_id,
                    SequenceStateSchema(
                        sequence_id=sequence_id,
                        status="completed",
                        run_id=run_id,
                        current_step_index=0,
                        total_steps=0,
                        completed_step_ids=[],
                        last_error=None,
                        started_at=started_at,
                        finished_at=datetime.now(timezone.utc),
                    ),
                )
                return

            for ctx in contexts:
                if cancel_event.is_set():
                    await self._mark_run_stopped(session, run_id, ctx.index)
                    await ws_manager.broadcast(
                        SequenceStoppedEvent(sequence_id=sequence_id, run_id=run_id)
                    )
                    self._cache_state(
                        sequence_id,
                        SequenceStateSchema(
                            sequence_id=sequence_id,
                            status="stopped",
                            run_id=run_id,
                            current_step_index=ctx.index,
                            total_steps=total_steps,
                            completed_step_ids=completed_step_ids,
                            last_error=None,
                            started_at=started_at,
                            finished_at=datetime.now(timezone.utc),
                        ),
                    )
                    elapsed_total = int((time.monotonic() - start_time) * 1000)
                    return

                step_started = time.monotonic()
                now = datetime.now(timezone.utc)
                await session.execute(
                    update(SequenceRunStep)
                    .where(SequenceRunStep.id == ctx.run_step_id)
                    .values(status=SequenceRunStepStatus.RUNNING, started_at=now)
                )
                await session.commit()

                try:
                    await self._execute_step(ctx)
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
                        step_started,
                        message,
                    )
                    await self._mark_run_error(session, run_id, ctx.index, message, start_time)
                    await ws_manager.broadcast(
                        SequenceStepErrorEvent(
                            sequence_id=sequence_id,
                            run_id=run_id,
                            step_index=ctx.index,
                            step_id=ctx.sequence_step_id,
                            message=message,
                        )
                    )
                    await ws_manager.broadcast(
                        SequenceErrorEvent(
                            sequence_id=sequence_id,
                            run_id=run_id,
                            message=f"Step {ctx.index + 1} failed",
                        )
                    )
                    self._cache_state(
                        sequence_id,
                        SequenceStateSchema(
                            sequence_id=sequence_id,
                            status="error",
                            run_id=run_id,
                            current_step_index=ctx.index,
                            total_steps=total_steps,
                            completed_step_ids=completed_step_ids,
                            last_error=message,
                            started_at=started_at,
                            finished_at=datetime.now(timezone.utc),
                        ),
                    )
                    elapsed_total = int((time.monotonic() - start_time) * 1000)
                    return

                await self._mark_step_status(
                    session,
                    ctx.run_step_id,
                    SequenceRunStepStatus.COMPLETED,
                    step_started,
                )
                completed_step_ids.append(ctx.sequence_step_id)
                await session.execute(
                    update(SequenceRun)
                    .where(SequenceRun.id == run_id)
                    .values(current_step_index=ctx.index + 1)
                )
                await session.commit()

                elapsed_total = int((time.monotonic() - start_time) * 1000)
                await ws_manager.broadcast(
                    SequenceProgressEvent(
                        sequence_id=sequence_id,
                        run_id=run_id,
                        step_index=ctx.index,
                        step_id=ctx.sequence_step_id,
                        step_type=ctx.step_type.value,
                        elapsed_ms=elapsed_total,
                        completed_steps=list(completed_step_ids),
                    )
                )
                self._cache_state(
                    sequence_id,
                    SequenceStateSchema(
                        sequence_id=sequence_id,
                        status="running",
                        run_id=run_id,
                        current_step_index=ctx.index + 1,
                        total_steps=total_steps,
                        completed_step_ids=list(completed_step_ids),
                        last_error=None,
                        started_at=started_at,
                        finished_at=None,
                    ),
                )

            await self._mark_run_completed(session, run_id, start_time)
            elapsed_total = int((time.monotonic() - start_time) * 1000)
            await ws_manager.broadcast(
                SequenceCompletedEvent(
                    sequence_id=sequence_id,
                    run_id=run_id,
                    elapsed_ms=elapsed_total,
                )
            )
            self._cache_state(
                sequence_id,
                SequenceStateSchema(
                    sequence_id=sequence_id,
                    status="completed",
                    run_id=run_id,
                    current_step_index=total_steps,
                    total_steps=total_steps,
                    completed_step_ids=list(completed_step_ids),
                    last_error=None,
                    started_at=started_at,
                    finished_at=datetime.now(timezone.utc),
                ),
            )
            logger.info(
                "Sequence run %s completed in %sms (%s steps)",
                run_id,
                elapsed_total,
                total_steps,
            )

    async def _execute_step(self, ctx: StepContext) -> None:
        step_type = ctx.step_type
        payload = ctx.payload or {}

        if step_type == SequenceStepType.WAIT:
            delay_ms = int(payload.get("ms", 0))
            if delay_ms > 0:
                await asyncio.sleep(delay_ms / 1000)
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

    async def _mark_run_completed(self, session, run_id: int, start_time: float) -> None:
        finished_at = datetime.now(timezone.utc)
        elapsed_ms = int((time.monotonic() - start_time) * 1000)
        await session.execute(
            update(SequenceRun)
            .where(SequenceRun.id == run_id)
            .values(
                status=SequenceRunStatus.COMPLETED,
                finished_at=finished_at,
                error_message=None,
            )
        )
        logger.info("Sequence run %s completed in %sms", run_id, elapsed_ms)
        await session.commit()

    async def _mark_run_error(
        self,
        session,
        run_id: int,
        step_index: int,
        message: str,
        start_time: float,
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
        await session.commit()

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
        await session.commit()

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
        await session.commit()

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

    async def _broadcast_started(self, sequence_id: int, run_id: int, total_steps: int) -> None:
        ws_manager = WebSocketManager.get_instance()
        await ws_manager.broadcast(
            SequenceStartedEvent(
                sequence_id=sequence_id,
                run_id=run_id,
                total_steps=total_steps,
            )
        )
