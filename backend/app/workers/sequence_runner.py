from __future__ import annotations

import asyncio
import time
from contextlib import suppress

from app.core.config import get_settings
from app.core.logger import get_logger
from app.core.sequence_dto import SequenceCommand, SequenceCommandType
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import enqueue_sequence_command, parse_sequence_command_entry
from app.services.worker_health import clear_worker_status, start_worker_heartbeat
from app.services.sequence_runner import (
    SequenceAlreadyRunningError,
    SequenceNotFoundError,
    SequenceRunner,
)
from app.workers.stream_worker_runtime import (
    build_worker_consumer_name,
    drain_pending_stream_entries,
    ensure_stream_consumer_group,
    fetch_stream_group_entries,
)
from app.workers.worker_lifecycle import install_stop_signal_handlers, run_consume_loop

settings = get_settings()
logger = get_logger("worker.sequence")

STREAM_NAME = settings.sequence_command_stream
GROUP_NAME = "sequence-runner"
CONSUMER_NAME = build_worker_consumer_name()
MAX_RETRIES = 5


async def _ensure_group(redis) -> None:
    await ensure_stream_consumer_group(
        redis,
        stream_name=STREAM_NAME,
        group_name=GROUP_NAME,
        logger=logger,
        create_label="sequence command",
        exists_label="Sequence command",
    )


async def _fetch(redis, stream_id: str, block_ms: int = 5000):
    return await fetch_stream_group_entries(
        redis,
        stream_name=STREAM_NAME,
        group_name=GROUP_NAME,
        consumer_name=CONSUMER_NAME,
        stream_id=stream_id,
        count=20,
        block_ms=block_ms,
    )


async def _drain_pending(redis, runner: SequenceRunner) -> None:
    await drain_pending_stream_entries(
        fetch_pending=lambda stream_id, block_ms: _fetch(redis, stream_id, block_ms=block_ms),
        process_entries=lambda entries: _process_entries(redis, runner, entries),
        logger=logger,
        replay_label="sequence commands",
        replay_limit=1000,
    )


async def _process_entries(redis, runner: SequenceRunner, entries) -> None:
    for entry_id, fields in entries:
        command: SequenceCommand | None = None
        started_monotonic: float | None = None
        try:
            _, command = parse_sequence_command_entry((entry_id, fields))
        except Exception as exc:  # noqa: BLE001
            logger.exception("💥 Invalid sequence command payload %s: %s", entry_id, exc)
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
            continue

        op_name = str(command.type.value if hasattr(command.type, "value") else command.type).lower()
        request_id = str(command.request_id or "").strip() or "-"
        bindings_count = len(_coerce_signal_bindings(command.extra)) if command.type == SequenceCommandType.START else 0
        started_monotonic = time.monotonic()
        logger.info(
            "▶️ Sequence command start | sequence=%s op=%s request=%s attempt=%s bindings=%s entry=%s",
            command.sequence_id,
            op_name,
            request_id,
            command.attempts,
            bindings_count,
            entry_id,
        )
        try:
            await _handle_command(runner, command)
            duration_ms = int((time.monotonic() - started_monotonic) * 1000) if started_monotonic else 0
            logger.info(
                "✅ Sequence command complete | sequence=%s op=%s request=%s attempt=%s duration=%sms entry=%s",
                command.sequence_id,
                op_name,
                request_id,
                command.attempts,
                duration_ms,
                entry_id,
            )
        except SequenceNotFoundError as exc:
            duration_ms = int((time.monotonic() - started_monotonic) * 1000) if started_monotonic else 0
            logger.error(
                "❌ Sequence command failed | sequence=%s op=%s request=%s attempt=%s duration=%sms err=%s entry=%s",
                command.sequence_id,
                op_name,
                request_id,
                command.attempts,
                duration_ms,
                str(exc),
                entry_id,
            )
            logger.error("💥 Sequence not found for command %s: %s", entry_id, exc)
        except SequenceAlreadyRunningError as exc:
            duration_ms = int((time.monotonic() - started_monotonic) * 1000) if started_monotonic else 0
            logger.warning(
                "⚠️ Sequence command rejected | sequence=%s op=%s request=%s attempt=%s duration=%sms reason=already_running entry=%s",
                command.sequence_id,
                op_name,
                request_id,
                command.attempts,
                duration_ms,
                entry_id,
            )
            logger.warning("⚠️ Sequence already running: %s", exc)
        except Exception as exc:  # noqa: BLE001
            duration_ms = int((time.monotonic() - started_monotonic) * 1000) if started_monotonic else 0
            logger.error(
                "❌ Sequence command failed | sequence=%s op=%s request=%s attempt=%s duration=%sms err=%s entry=%s",
                command.sequence_id,
                op_name,
                request_id,
                command.attempts,
                duration_ms,
                str(exc),
                entry_id,
            )
            logger.exception("💥 Failed to process sequence command %s: %s", entry_id, exc)
            if command:
                await _retry_or_dlq(command, exc)
        finally:
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)


async def _handle_command(runner: SequenceRunner, command: SequenceCommand) -> None:
    if command.type == SequenceCommandType.START:
        signal_bindings = _coerce_signal_bindings(command.extra)
        await runner.start(
            command.sequence_id,
            request_id=command.request_id,
            requested_by=command.requested_by,
            signal_bindings=signal_bindings,
        )
        return
    if command.type == SequenceCommandType.STOP:
        await runner.stop(command.sequence_id)
        return
    raise ValueError(f"Unknown sequence command type: {command.type}")


def _coerce_signal_bindings(extra: dict | None) -> dict[str, int]:
    if not isinstance(extra, dict):
        return {}
    raw = extra.get("signal_bindings")
    if not isinstance(raw, dict):
        return {}

    bindings: dict[str, int] = {}
    for key, value in raw.items():
        if not isinstance(key, str) or not key.strip():
            continue
        try:
            bindings[key.strip()] = int(value)
        except (TypeError, ValueError):
            continue
    return bindings


async def _retry_or_dlq(command: SequenceCommand, error: Exception) -> bool:
    next_attempt = command.attempts + 1
    reason = str(error)
    if next_attempt <= MAX_RETRIES:
        retry_cmd = command.bumped_attempt(next_attempt, extra={"last_error": reason})
        await enqueue_sequence_command(retry_cmd)
        logger.warning(
            "🔁 Requeued sequence command (sequence=%s, attempt=%s/%s)",
            command.sequence_id,
            next_attempt,
            MAX_RETRIES,
        )
        return False
    else:
        dlq_cmd = command.bumped_attempt(next_attempt, extra={"dlq_reason": reason})
        await enqueue_sequence_command(dlq_cmd, stream_name=settings.sequence_command_dlq_stream)
        logger.error(
            "💀 Command moved to DLQ after %s attempts (sequence=%s)",
            next_attempt,
            command.sequence_id,
        )
        return True


async def main() -> None:
    await RedisManager.start()
    redis = RedisManager.get_instance()

    await _ensure_group(redis)

    runner = SequenceRunner()

    await _drain_pending(redis, runner)
    heartbeat_task = start_worker_heartbeat("sequence_runner")

    stop_event = asyncio.Event()
    install_stop_signal_handlers(
        stop_event=stop_event,
        logger=logger,
        stop_message="🛑 Stop signal received, shutting down sequence runner worker...",
    )

    logger.info(
        "🚀 Sequence runner worker ready (stream=%s, group=%s, consumer=%s)",
        STREAM_NAME,
        GROUP_NAME,
        CONSUMER_NAME,
    )

    try:
        await run_consume_loop(
            stop_event=stop_event,
            fetch_entries=lambda: _fetch(redis, ">"),
            process_entries=lambda entries: _process_entries(redis, runner, entries),
            logger=logger,
        )
    finally:
        heartbeat_task.cancel()
        with suppress(asyncio.CancelledError):
            await heartbeat_task
        await clear_worker_status("sequence_runner")
        await RedisManager.stop()


if __name__ == "__main__":
    asyncio.run(main())
