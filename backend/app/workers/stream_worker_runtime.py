from __future__ import annotations

import os
import socket
from typing import Protocol
from collections.abc import Awaitable, Callable

from redis.exceptions import ResponseError
from app.infrastructure.redis.types import RedisStreamClient, RedisStreamEntries


class WorkerLogger(Protocol):
    def info(self, msg: object, *args: object) -> None: ...

    def warning(self, msg: object, *args: object) -> None: ...


def build_worker_consumer_name() -> str:
    return f"{socket.gethostname()}-{os.getpid()}"


async def ensure_stream_consumer_group(
    redis: RedisStreamClient,
    *,
    stream_name: str,
    group_name: str,
    logger: WorkerLogger,
    create_label: str,
    exists_label: str | None = None,
) -> None:
    try:
        _ = await redis.xgroup_create(stream_name, group_name, id="0", mkstream=True)
        _ = logger.info("✅ Created %s consumer group %s", create_label, group_name)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            _ = logger.info("ℹ️ %s consumer group already exists", exists_label or create_label)
        else:
            raise


async def fetch_stream_group_entries(
    redis: RedisStreamClient,
    *,
    stream_name: str,
    group_name: str,
    consumer_name: str,
    stream_id: str,
    count: int,
    block_ms: int = 5000,
) -> RedisStreamEntries:
    result = await redis.xreadgroup(
        group_name,
        consumer_name,
        streams={stream_name: stream_id},
        count=count,
        block=block_ms,
    )
    if not result:
        return []
    return result[0][1]


async def drain_pending_stream_entries(
    *,
    fetch_pending: Callable[[str, int], Awaitable[RedisStreamEntries]],
    process_entries: Callable[[RedisStreamEntries], Awaitable[None]],
    logger: WorkerLogger,
    replay_label: str,
    replay_limit: int = 1000,
) -> None:
    replayed = 0
    while replayed < replay_limit:
        entries = await fetch_pending("0", 100)
        if not entries:
            break
        replayed += len(entries)
        _ = logger.info("🔁 Replaying %d pending %s", len(entries), replay_label)
        await process_entries(entries)
    if replayed >= replay_limit:
        _ = logger.warning(
            "⚠️ Pending replay limit reached (%d), leaving remaining pending entries for next cycle",
            replay_limit,
        )
