from __future__ import annotations

import asyncio
from contextlib import suppress
import logging

from .config import AgentConfig
from .diag_collector import DiagnosticsCollector
from .models import CommandEnvelope, CoreDiagSnapshot, CpuDiagnostics, DiskDiagnostics, MemoryDiagnostics
from .redis_protocol import RedisProtocol

logger = logging.getLogger("unitlab.core_diag_agent")


class CoreDiagAgent:
    def __init__(self, config: AgentConfig) -> None:
        self.config = config
        self.redis = RedisProtocol(config)
        self.collector = DiagnosticsCollector(config)
        self._stop = asyncio.Event()
        self._status_task: asyncio.Task[None] | None = None
        self._command_task: asyncio.Task[None] | None = None
        self._snapshot = CoreDiagSnapshot(
            mode="unknown",
            hostname=None,
            model=None,
            os_pretty_name=None,
            kernel=None,
            time_utc=None,
            uptime_seconds=None,
            cpu=CpuDiagnostics(None, None, None, None),
            memory=MemoryDiagnostics(None, None, None, None),
            disk_root=DiskDiagnostics("/", None, None, None, None),
            services=[],
        )

    async def start(self) -> None:
        await self.redis.ensure_group()
        await self._refresh_status(publish=True, last_event="agent_started")
        self._status_task = asyncio.create_task(self._status_loop(), name="unitlab-core-diag-status")
        self._command_task = asyncio.create_task(self._command_loop(), name="unitlab-core-diag-commands")
        logger.info("UnitLab RPi core diagnostics agent started | consumer=%s", self.config.redis_consumer_name)

    async def stop(self) -> None:
        self._stop.set()
        for task in (self._status_task, self._command_task):
            if task:
                task.cancel()
        for task in (self._status_task, self._command_task):
            if task:
                with suppress(asyncio.CancelledError):
                    await task
        await self.redis.close()

    async def _status_loop(self) -> None:
        while not self._stop.is_set():
            try:
                await self._refresh_status(publish=True)
            except asyncio.CancelledError:
                raise
            except Exception as exc:  # noqa: BLE001
                logger.exception("Status loop failed: %s", exc)
                await self._publish_error("status_loop_error", str(exc))
            await asyncio.sleep(self.config.status_publish_interval_sec)

    async def _command_loop(self) -> None:
        while not self._stop.is_set():
            try:
                commands = await self.redis.read_commands()
                for cmd in commands:
                    await self._handle_command(cmd)
            except asyncio.CancelledError:
                raise
            except Exception as exc:  # noqa: BLE001
                logger.exception("Command loop failed: %s", exc)
                await self._publish_error("command_loop_error", str(exc))
                await asyncio.sleep(1)

    async def _handle_command(self, cmd: CommandEnvelope) -> None:
        logger.info("Command received | request=%s action=%s entry=%s", cmd.request_id, cmd.action, cmd.entry_id)
        await self._set_request_in_flight(cmd)
        try:
            action = cmd.action.strip().lower()
            if action == "status":
                await self._refresh_status(publish=True, request_id=cmd.request_id, last_event="status")
            else:
                await self.redis.publish_event(
                    "command_rejected",
                    {
                        "request_id": cmd.request_id,
                        "entry_id": cmd.entry_id,
                        "action": cmd.action,
                        "reason": f"Unknown action: {cmd.action}",
                    },
                )
        except Exception as exc:  # noqa: BLE001
            logger.exception("Command failed | request=%s action=%s", cmd.request_id, cmd.action)
            await self.redis.publish_event(
                "command_failed",
                {"request_id": cmd.request_id, "entry_id": cmd.entry_id, "action": cmd.action, "error": str(exc)},
            )
            self._snapshot.mode = "error"
            self._snapshot.last_error = str(exc)
            await self._publish_snapshot(last_event="command_failed", request_id=cmd.request_id)
        finally:
            await self._clear_request_in_flight()
            await self.redis.ack(cmd.entry_id)

    async def _refresh_status(self, *, publish: bool, request_id: str | None = None, last_event: str | None = None) -> None:
        collected = await self.collector.collect()
        self._snapshot = collected.snapshot
        if publish:
            await self._publish_snapshot(last_event=last_event, request_id=request_id)

    async def _publish_snapshot(self, *, last_event: str | None = None, request_id: str | None = None) -> None:
        self._snapshot.last_event = last_event or self._snapshot.last_event
        snapshot = self._snapshot.to_dict()
        if request_id:
            snapshot["request_id"] = request_id
        await self.redis.set_state(snapshot)
        await self.redis.publish_event("state", snapshot)

    async def _publish_error(self, event_type: str, error: str) -> None:
        self._snapshot.mode = "error"
        self._snapshot.last_error = error
        await self._publish_snapshot(last_event=event_type)
        await self.redis.publish_event(event_type, {"error": error})

    async def _set_request_in_flight(self, cmd: CommandEnvelope) -> None:
        self._snapshot.request_in_flight = {"request_id": cmd.request_id, "entry_id": cmd.entry_id, "action": cmd.action}
        await self._publish_snapshot(last_event="command_started", request_id=cmd.request_id)

    async def _clear_request_in_flight(self) -> None:
        self._snapshot.request_in_flight = None
        await self._publish_snapshot(last_event="command_finished")

