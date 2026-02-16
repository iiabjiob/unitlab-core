from __future__ import annotations

import asyncio
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any, Awaitable, Callable, Dict, List, Optional

from app.infrastructure.protocol.modes import Cmd
from app.models.sequence.types import SequenceStepType
from app.services.command_queue_service import enqueue_ao_command, enqueue_do_command
from app.services.domain_errors import DomainError, SequenceNotApplicableError


class SequenceCancellationRequested(Exception):
    """Raised internally to unwind step execution when cancellation is signalled."""


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
    step_type: SequenceStepType
    payload: Dict[str, Any]
    primary_channel: Optional[ChannelInfo]
    pair_channels: List[ChannelInfo]
    target_device: Optional[DeviceInfo]
    run_step_id: Optional[int] = None


@dataclass(frozen=True)
class RunStartedEvent:
    total_steps: int
    started_at: datetime
    start_time: float


@dataclass(frozen=True)
class StepLifecycleEvent:
    context: StepContext
    started_at: datetime
    started_monotonic: float


@dataclass(frozen=True)
class StepCompletedEvent(StepLifecycleEvent):
    completed_step_ids: List[int]
    total_steps: int
    step_elapsed_ms: int
    run_elapsed_ms: int


@dataclass(frozen=True)
class StepFailedEvent(StepLifecycleEvent):
    message: str
    exception: Optional[BaseException] = None


@dataclass(frozen=True)
class CancellationEvent:
    current_step: Optional[StepContext]
    started_monotonic: Optional[float]
    fallback_index: int


@dataclass(frozen=True)
class SequenceExecutionResult:
    status: str
    current_step_index: int
    completed_step_ids: List[int]
    started_at: datetime
    finished_at: datetime
    last_error: Optional[str] = None


RunStartedHook = Callable[[RunStartedEvent], Awaitable[None]]
StepLifecycleHook = Callable[[StepLifecycleEvent], Awaitable[None]]
StepCompletedHook = Callable[[StepCompletedEvent], Awaitable[None]]
StepFailedHook = Callable[[StepFailedEvent], Awaitable[None]]
CancellationHook = Callable[[CancellationEvent], Awaitable[int]]
RunFinishedHook = Callable[[SequenceExecutionResult], Awaitable[None]]
CancellationProbe = Callable[[], Awaitable[None]]


async def _noop_async(*_args, **_kwargs) -> None:  # pragma: no cover - trivial
    return None


async def _default_cancellation_hook(payload: CancellationEvent) -> int:
    return payload.fallback_index


@dataclass
class SequenceExecutorHooks:
    on_run_started: RunStartedHook = _noop_async
    on_step_started: StepLifecycleHook = _noop_async
    on_step_completed: StepCompletedHook = _noop_async
    on_step_failed: StepFailedHook = _noop_async
    on_cancellation: CancellationHook = _default_cancellation_hook
    on_finished: RunFinishedHook = _noop_async


class SequenceExecutor:
    """Context-free sequence orchestrator that only knows how to execute steps."""

    def __init__(self) -> None:
        self._cancellation_probe_min_interval = 0.3
        self._last_probe_at: Dict[int, float] = {}

    async def run(
        self,
        *,
        contexts: List[StepContext],
        cancel_event: asyncio.Event,
        hooks: Optional[SequenceExecutorHooks] = None,
        cancellation_probe: Optional[CancellationProbe] = None,
    ) -> SequenceExecutionResult:
        hooks = hooks or SequenceExecutorHooks()
        total_steps = len(contexts)
        started_at = datetime.now(timezone.utc)
        start_time = time.monotonic()

        await hooks.on_run_started(
            RunStartedEvent(total_steps=total_steps, started_at=started_at, start_time=start_time)
        )

        completed_step_ids: List[int] = []

        async def _probe() -> None:
            if not cancellation_probe:
                return
            now = time.monotonic()
            last_checked = self._last_probe_at.get(id(cancel_event))
            if last_checked is not None and now - last_checked < self._cancellation_probe_min_interval:
                return
            self._last_probe_at[id(cancel_event)] = now
            await cancellation_probe()

        for ctx in contexts:
            await _probe()
            if cancel_event.is_set():
                result = await self._handle_cancellation(ctx, completed_step_ids, hooks, started_at, start_time)
                self._last_probe_at.pop(id(cancel_event), None)
                return result

            step_started_at = datetime.now(timezone.utc)
            step_started_monotonic = time.monotonic()
            lifecycle = StepLifecycleEvent(
                context=ctx,
                started_at=step_started_at,
                started_monotonic=step_started_monotonic,
            )
            await hooks.on_step_started(lifecycle)

            try:
                await self._execute_step(ctx, cancel_event, _probe)
            except SequenceCancellationRequested:
                cancel_event.set()
                result = await self._handle_cancellation(
                    ctx,
                    completed_step_ids,
                    hooks,
                    started_at,
                    start_time,
                    step_started_monotonic,
                )
                self._last_probe_at.pop(id(cancel_event), None)
                return result
            except DomainError as exc:
                failure = StepFailedEvent(
                    context=ctx,
                    started_at=step_started_at,
                    started_monotonic=step_started_monotonic,
                    message=str(exc),
                )
                await hooks.on_step_failed(failure)
                finished_at = datetime.now(timezone.utc)
                result = SequenceExecutionResult(
                    status="error",
                    current_step_index=ctx.index,
                    completed_step_ids=list(completed_step_ids),
                    started_at=started_at,
                    finished_at=finished_at,
                    last_error=failure.message,
                )
                await hooks.on_finished(result)
                self._last_probe_at.pop(id(cancel_event), None)
                return result
            except Exception as exc:  # noqa: BLE001
                failure = StepFailedEvent(
                    context=ctx,
                    started_at=step_started_at,
                    started_monotonic=step_started_monotonic,
                    message=str(exc),
                    exception=exc,
                )
                await hooks.on_step_failed(failure)
                finished_at = datetime.now(timezone.utc)
                result = SequenceExecutionResult(
                    status="error",
                    current_step_index=ctx.index,
                    completed_step_ids=list(completed_step_ids),
                    started_at=started_at,
                    finished_at=finished_at,
                    last_error=failure.message,
                )
                await hooks.on_finished(result)
                self._last_probe_at.pop(id(cancel_event), None)
                return result

            completed_step_ids.append(ctx.sequence_step_id)
            step_elapsed_ms = int((time.monotonic() - step_started_monotonic) * 1000)
            run_elapsed_ms = int((time.monotonic() - start_time) * 1000)
            await hooks.on_step_completed(
                StepCompletedEvent(
                    context=ctx,
                    started_at=step_started_at,
                    started_monotonic=step_started_monotonic,
                    completed_step_ids=list(completed_step_ids),
                    total_steps=total_steps,
                    step_elapsed_ms=step_elapsed_ms,
                    run_elapsed_ms=run_elapsed_ms,
                )
            )

        finished_at = datetime.now(timezone.utc)
        result = SequenceExecutionResult(
            status="completed",
            current_step_index=total_steps,
            completed_step_ids=list(completed_step_ids),
            started_at=started_at,
            finished_at=finished_at,
            last_error=None,
        )
        await hooks.on_finished(result)
        self._last_probe_at.pop(id(cancel_event), None)
        return result

    async def _handle_cancellation(
        self,
        ctx: StepContext,
        completed_step_ids: List[int],
        hooks: SequenceExecutorHooks,
        started_at: datetime,
        start_time: float,
        step_started_monotonic: Optional[float] = None,
    ) -> SequenceExecutionResult:
        cancellation_index = await hooks.on_cancellation(
            CancellationEvent(
                current_step=ctx,
                started_monotonic=step_started_monotonic,
                fallback_index=len(completed_step_ids),
            )
        )
        finished_at = datetime.now(timezone.utc)
        result = SequenceExecutionResult(
            status="stopped",
            current_step_index=cancellation_index,
            completed_step_ids=list(completed_step_ids),
            started_at=started_at,
            finished_at=finished_at,
            last_error="stopped",
        )
        await hooks.on_finished(result)
        return result

    async def _execute_step(
        self,
        ctx: StepContext,
        cancel_event: asyncio.Event,
        cancellation_probe: CancellationProbe,
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
                raise SequenceNotApplicableError("AO_SET step requires a primary channel")
            value = float(payload.get("value", 0))
            await enqueue_ao_command(
                unit_id=ctx.primary_channel.unit_id,
                ch=ctx.primary_channel.channel_index,
                value=value,
            )
            return

        if step_type == SequenceStepType.DO_LATCH:
            if not ctx.primary_channel:
                raise SequenceNotApplicableError("DO_LATCH step requires a primary channel")
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
                raise SequenceNotApplicableError("DO_PULSE step requires a primary channel")
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
                raise SequenceNotApplicableError("DO_PAIR step requires two channels")
            first, second = ctx.pair_channels
            if first.unit_id != second.unit_id:
                raise SequenceNotApplicableError("Pair channels must belong to the same device")
            if first.id == second.id or first.channel_index == second.channel_index:
                raise SequenceNotApplicableError("Pair channels must be different channels")
            raw_state = payload.get("state2b", 0)
            try:
                state2b = int(raw_state)
            except (TypeError, ValueError) as exc:
                raise SequenceNotApplicableError("DO_PAIR state must be an integer in range 0..3") from exc
            if state2b < 0 or state2b > 3:
                raise SequenceNotApplicableError("DO_PAIR state must be in range 0..3")
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
                raise SequenceNotApplicableError("DO_BITMASK step requires device context")
            bitmask = int(payload.get("bitmask", 0))
            await enqueue_do_command(
                unit_id=ctx.target_device.unit_id,
                mode=Cmd.SET_ALL_BIT,
                bitmask=bitmask,
            )
            return

        raise SequenceNotApplicableError(f"Unsupported step type {step_type}")

    async def _wait_with_cancellation(
        self,
        cancel_event: asyncio.Event,
        delay_ms: int,
        cancellation_probe: CancellationProbe,
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


__all__ = [
    "CancellationEvent",
    "ChannelInfo",
    "DeviceInfo",
    "RunStartedEvent",
    "SequenceCancellationRequested",
    "SequenceExecutionResult",
    "SequenceExecutor",
    "SequenceExecutorHooks",
    "StepCompletedEvent",
    "StepContext",
    "StepFailedEvent",
    "StepLifecycleEvent",
]
