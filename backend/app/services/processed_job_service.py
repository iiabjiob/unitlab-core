from __future__ import annotations

from datetime import datetime, timezone

from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession


async def has_processed_job_marker(
    session: AsyncSession,
    *,
    worker_name: str,
    job_id: str,
) -> bool:
    result = await session.execute(
        text(
            """
            SELECT 1
            FROM processed_jobs
            WHERE worker_name = :worker_name
              AND job_id = :job_id
            LIMIT 1
            """
        ),
        {
            "worker_name": worker_name,
            "job_id": job_id,
        },
    )
    return result.scalar_one_or_none() is not None


async def write_processed_job_marker_best_effort(
    session: AsyncSession,
    *,
    worker_name: str,
    job_id: str,
    stream_name: str,
    entry_id: str | None = None,
) -> None:
    # Best-effort marker write for long-running workflows where a single
    # transaction cannot cover the whole execution (e.g. test runs with sleeps/IO).
    await session.execute(
        text(
            """
            INSERT INTO processed_jobs (worker_name, job_id, stream_name, entry_id, processed_at)
            VALUES (:worker_name, :job_id, :stream_name, :entry_id, :processed_at)
            ON CONFLICT (worker_name, job_id) DO NOTHING
            """
        ),
        {
            "worker_name": worker_name,
            "job_id": job_id,
            "stream_name": stream_name,
            "entry_id": entry_id,
            "processed_at": datetime.now(timezone.utc),
        },
    )


async def try_acquire_processed_job(
    session: AsyncSession,
    *,
    worker_name: str,
    job_id: str,
    stream_name: str,
    entry_id: str | None = None,
) -> bool:
    # Atomic idempotency guard for short/atomic jobs.
    # Must be used inside the same transaction as the domain write operations.
    result = await session.execute(
        text(
            """
            INSERT INTO processed_jobs (worker_name, job_id, stream_name, entry_id, processed_at)
            VALUES (:worker_name, :job_id, :stream_name, :entry_id, :processed_at)
            ON CONFLICT (worker_name, job_id) DO NOTHING
            RETURNING 1
            """
        ),
        {
            "worker_name": worker_name,
            "job_id": job_id,
            "stream_name": stream_name,
            "entry_id": entry_id,
            "processed_at": datetime.now(timezone.utc),
        },
    )
    return result.scalar_one_or_none() is not None
