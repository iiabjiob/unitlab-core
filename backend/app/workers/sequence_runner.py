from __future__ import annotations

import asyncio
import os
import signal
import socket

from redis.exceptions import ResponseError

from app.core.config import get_settings
from app.core.logger import get_logger
from app.core.sequence_dto import SequenceCommand, SequenceCommandType
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import enqueue_sequence_command, parse_sequence_command_entry
from app.services.sequence_runner import (
    SequenceAlreadyRunningError,
    SequenceNotFoundError,
    SequenceRunner,
)
from app.services.test_run_service import TestRunNotFoundError, TestRunService
from app.services.domain_errors import DomainError, TestRunInvalidStateError
from app.infrastructure.db.database import AsyncSessionLocal

settings = get_settings()
logger = get_logger("worker.sequence")

STREAM_NAME = settings.sequence_command_stream
GROUP_NAME = "sequence-runner"
CONSUMER_NAME = f"{socket.gethostname()}-{os.getpid()}"
MAX_RETRIES = 5


async def _ensure_group(redis) -> None:
    try:
        await redis.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        logger.info("✅ Created sequence command consumer group %s", GROUP_NAME)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            logger.info("ℹ️ Sequence command consumer group already exists")
        else:
            raise


async def _fetch(redis, stream_id: str, block_ms: int = 5000):
    result = await redis.xreadgroup(
        GROUP_NAME,
        CONSUMER_NAME,
        streams={STREAM_NAME: stream_id},
        count=20,
        block=block_ms,
    )
    if not result:
        return []
    return result[0][1]


async def _drain_pending(redis, runner: SequenceRunner) -> None:
    while True:
        entries = await _fetch(redis, "0", block_ms=100)
        if not entries:
            break
        logger.info("🔁 Replaying %d pending sequence commands", len(entries))
        await _process_entries(redis, runner, entries)


async def _process_entries(redis, runner: SequenceRunner, entries) -> None:
    for entry_id, fields in entries:
        command: SequenceCommand | None = None
        try:
            _, command = parse_sequence_command_entry((entry_id, fields))
        except Exception as exc:  # noqa: BLE001
            logger.exception("💥 Invalid sequence command payload %s: %s", entry_id, exc)
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
            continue

        try:
            await _handle_command(runner, command)
        except SequenceNotFoundError as exc:
            logger.error("💥 Sequence not found for command %s: %s", entry_id, exc)
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
        except TestRunNotFoundError as exc:
            logger.error("💥 Test run invalid for command %s: %s", entry_id, exc)
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
        except DomainError as exc:
            if isinstance(exc, TestRunInvalidStateError):
                logger.warning("⚠️ Test run state rejected for command %s: %s", entry_id, exc)
            else:
                await _mark_test_run_failed(command, exc)
                logger.warning("⚠️ Invalid test run command %s: %s", entry_id, exc)
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
        except SequenceAlreadyRunningError as exc:
            logger.warning("⚠️ Sequence already running: %s", exc)
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
        except Exception as exc:  # noqa: BLE001
            logger.exception("💥 Failed to process sequence command %s: %s", entry_id, exc)
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
            if command:
                moved_to_dlq = await _retry_or_dlq(command, exc)
                if moved_to_dlq:
                    await _mark_test_run_failed(command, exc)
        else:
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)


async def _mark_test_run_failed(command: SequenceCommand | None, error: Exception) -> None:
    if not command or command.test_run_id is None:
        return
    async with AsyncSessionLocal() as session:
        service = TestRunService(session)
        await service.mark_failed(command.test_run_id, str(error))


async def _handle_command(runner: SequenceRunner, command: SequenceCommand) -> None:
    if command.type == SequenceCommandType.START:
        if command.test_run_id is None:
            raise ValueError("START command requires test_run_id")
        await runner.start_test_run(
            command.test_run_id,
            request_id=command.request_id,
            requested_by=command.requested_by,
        )
        return
    if command.type == SequenceCommandType.STOP:
        if command.test_run_id is None:
            raise ValueError("STOP command requires test_run_id")
        await runner.stop_test_run(command.test_run_id)
        return
    raise ValueError(f"Unknown sequence command type: {command.type}")


async def _retry_or_dlq(command: SequenceCommand, error: Exception) -> bool:
    next_attempt = command.attempts + 1
    reason = str(error)
    if next_attempt <= MAX_RETRIES:
        retry_cmd = command.bumped_attempt(next_attempt, extra={"last_error": reason})
        await enqueue_sequence_command(retry_cmd)
        logger.warning(
            "🔁 Requeued sequence command (test_run=%s, attempt=%s/%s)",
            command.test_run_id,
            next_attempt,
            MAX_RETRIES,
        )
        return False
    else:
        dlq_cmd = command.bumped_attempt(next_attempt, extra={"dlq_reason": reason})
        await enqueue_sequence_command(dlq_cmd, stream_name=settings.sequence_command_dlq_stream)
        logger.error(
            "💀 Command moved to DLQ after %s attempts (test_run=%s)",
            next_attempt,
            command.test_run_id,
        )
        return True


async def main() -> None:
    await RedisManager.start()
    redis = RedisManager.get_instance()

    await _ensure_group(redis)

    runner = SequenceRunner()

    await _drain_pending(redis, runner)

    stop_event = asyncio.Event()

    def _signal_handler() -> None:
        logger.info("🛑 Stop signal received, shutting down sequence runner worker...")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(sig, _signal_handler)
        except NotImplementedError:
            pass

    logger.info(
        "🚀 Sequence runner worker ready (stream=%s, group=%s, consumer=%s)",
        STREAM_NAME,
        GROUP_NAME,
        CONSUMER_NAME,
    )

    try:
        while not stop_event.is_set():
            entries = await _fetch(redis, ">")
            if not entries:
                continue
            await _process_entries(redis, runner, entries)
    finally:
        await RedisManager.stop()


if __name__ == "__main__":
    asyncio.run(main())
