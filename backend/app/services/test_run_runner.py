"""Async runner that executes channel-based test runs."""
from __future__ import annotations

import asyncio
import contextlib
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import List

from sqlalchemy import func, select, update
from sqlalchemy.orm import selectinload

from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.protocol.modes import Cmd
from app.models.channel import Channel
from app.models.test_run import TestRun, TestRunStatus, TestRunStep
from app.schemas.test_run_schema import TestRunSettings
from app.services.command_queue_service import enqueue_do_command

logger = get_logger("test-runner")


class TestRunRunnerError(Exception):
    """Base class for test run runner errors."""


class TestRunAlreadyRunningError(TestRunRunnerError):
    pass


class TestRunNotRunningError(TestRunRunnerError):
    pass


class TestRunValidationError(TestRunRunnerError):
    pass


@dataclass(frozen=True)
class StepContext:
    channel_index: int
    unit_id: str


@dataclass
class RunContext:
    run_id: int
    settings: TestRunSettings
    steps: List[StepContext]
    unit_ids: List[str]


@dataclass
class ActiveRun:
    run_id: int
    cancel_event: asyncio.Event
    task: asyncio.Task[None]


class TestRunRunner:
    _instance: "TestRunRunner | None" = None

    def __init__(self) -> None:
        self._lock = asyncio.Lock()
        self._active_runs: dict[int, ActiveRun] = {}

    @classmethod
    def get_instance(cls) -> "TestRunRunner":
        if cls._instance is None:
            cls._instance = TestRunRunner()
        return cls._instance

    @classmethod
    def reset_instance(cls) -> None:  # pragma: no cover - helper for tests
        cls._instance = None

    async def start(self, project_id: int, run_id: int) -> None:
        async with self._lock:
            if run_id in self._active_runs:
                raise TestRunAlreadyRunningError(f"Test run {run_id} already running")
            context = await self._prepare_context(project_id, run_id)
            cancel_event = asyncio.Event()
            task = asyncio.create_task(
                self._execute(context, cancel_event),
                name=f"test-run-{run_id}",
            )
            self._active_runs[run_id] = ActiveRun(run_id, cancel_event, task)
            task.add_done_callback(lambda _: self._active_runs.pop(run_id, None))

    async def cancel(self, run_id: int) -> None:
        async with self._lock:
            handle = self._active_runs.get(run_id)
            if not handle:
                raise TestRunNotRunningError(f"Test run {run_id} not running")
            handle.cancel_event.set()
            task = handle.task
        with contextlib.suppress(Exception):  # type: ignore[name-defined]
            await task

    async def _prepare_context(self, project_id: int, run_id: int) -> RunContext:
        async with AsyncSessionLocal() as session:
            stmt = (
                select(TestRun)
                .options(
                    selectinload(TestRun.steps)
                    .selectinload(TestRunStep.channel)
                    .selectinload(Channel.device)
                )
                .where(TestRun.id == run_id, TestRun.project_id == project_id)
            )
            result = await session.execute(stmt)
            run = result.scalar_one_or_none()
            if not run:
                raise TestRunRunnerError(f"Test run {run_id} not found")
            if run.status == TestRunStatus.RUNNING:
                raise TestRunAlreadyRunningError(f"Test run {run_id} already running")

            steps = sorted(run.steps, key=lambda step: step.order_index)
            if not steps:
                raise TestRunValidationError("Test run has no channels configured")

            contexts: list[StepContext] = []
            unit_ids: list[str] = []
            for step in steps:
                channel = step.channel
                if not channel or not channel.device:
                    raise TestRunValidationError("Each step must have a channel bound to a device")
                contexts.append(
                    StepContext(
                        channel_index=channel.channel_index,
                        unit_id=channel.device.unit_id,
                    )
                )
                if channel.device.unit_id not in unit_ids:
                    unit_ids.append(channel.device.unit_id)

            settings = TestRunSettings.model_validate(run.settings or {})
            await session.execute(
                update(TestRun)
                .where(TestRun.id == run_id)
                .values(
                    status=TestRunStatus.RUNNING,
                    started_at=datetime.now(timezone.utc),
                    finished_at=None,
                    error_message=None,
                    current_step_index=0,
                )
            )
            await session.commit()

        return RunContext(
            run_id=run_id,
            settings=settings,
            steps=contexts,
            unit_ids=unit_ids,
        )

    async def _execute(self, context: RunContext, cancel_event: asyncio.Event) -> None:
        delay_seconds = max(context.settings.delay_ms, 0) / 1000
        try:
            await self._reset_devices(context.unit_ids, cancel_event)
            await self._pause(delay_seconds, cancel_event)

            for index, step in enumerate(context.steps):
                if cancel_event.is_set():
                    await self._mark_cancelled(context.run_id, index)
                    return
                await self._activate_channel(step)
                await self._pause(delay_seconds, cancel_event)
                if cancel_event.is_set():
                    await self._mark_cancelled(context.run_id, index)
                    return
                await self._deactivate_channel(step)
                await self._pause(delay_seconds, cancel_event)
                await self._mark_progress(context.run_id, index + 1)

            await self._mark_completed(context.run_id, len(context.steps))
        except Exception as exc:  # noqa: BLE001
            logger.exception("Test run %s failed", context.run_id)
            await self._mark_failed(context.run_id, exc)

    async def _reset_devices(self, unit_ids: List[str], cancel_event: asyncio.Event) -> None:
        for unit_id in unit_ids:
            if cancel_event.is_set():
                return
            await enqueue_do_command(
                unit_id=unit_id,
                mode=Cmd.SET_ALL_BIT,
                bitmask=0,
            )

    async def _activate_channel(self, step: StepContext) -> None:
        await enqueue_do_command(
            unit_id=step.unit_id,
            mode=Cmd.SET_SINGLE_BIT,
            ch=step.channel_index,
            value=1,
        )

    async def _deactivate_channel(self, step: StepContext) -> None:
        await enqueue_do_command(
            unit_id=step.unit_id,
            mode=Cmd.SET_SINGLE_BIT,
            ch=step.channel_index,
            value=0,
        )

    async def _pause(self, delay_seconds: float, cancel_event: asyncio.Event) -> None:
        if delay_seconds <= 0:
            return
        try:
            await asyncio.wait_for(cancel_event.wait(), timeout=delay_seconds)
        except asyncio.TimeoutError:
            return

    async def _mark_progress(self, run_id: int, current_index: int) -> None:
        async with AsyncSessionLocal() as session:
            await session.execute(
                update(TestRun)
                .where(TestRun.id == run_id)
                .values(current_step_index=current_index, updated_at=func.now())
            )
            await session.commit()

    async def _mark_completed(self, run_id: int, total_steps: int) -> None:
        finished_at = datetime.now(timezone.utc)
        async with AsyncSessionLocal() as session:
            await session.execute(
                update(TestRun)
                .where(TestRun.id == run_id)
                .values(
                    status=TestRunStatus.COMPLETED,
                    finished_at=finished_at,
                    current_step_index=total_steps,
                    updated_at=func.now(),
                )
            )
            await session.commit()
        logger.info("Test run %s completed", run_id)

    async def _mark_cancelled(self, run_id: int, current_index: int) -> None:
        finished_at = datetime.now(timezone.utc)
        async with AsyncSessionLocal() as session:
            await session.execute(
                update(TestRun)
                .where(TestRun.id == run_id)
                .values(
                    status=TestRunStatus.CANCELLED,
                    finished_at=finished_at,
                    current_step_index=current_index,
                    updated_at=func.now(),
                )
            )
            await session.commit()
        logger.info("Test run %s cancelled", run_id)

    async def _mark_failed(self, run_id: int, error: Exception) -> None:
        finished_at = datetime.now(timezone.utc)
        async with AsyncSessionLocal() as session:
            await session.execute(
                update(TestRun)
                .where(TestRun.id == run_id)
                .values(
                    status=TestRunStatus.FAILED,
                    finished_at=finished_at,
                    error_message=str(error),
                    updated_at=func.now(),
                )
            )
            await session.commit()
