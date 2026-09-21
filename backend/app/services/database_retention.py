from __future__ import annotations

import asyncio
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from typing import Any, cast

from sqlalchemy import delete, func, select, text, tuple_
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.models.core_diagnostics import CoreDiagnosticsAcknowledgement
from app.models.hardware_command import HardwareCommandIntent
from app.models.processed_job import ProcessedJob


logger = get_logger("service.database_retention")

# Session-level advisory lock. The value is intentionally stable across API instances.
RETENTION_ADVISORY_LOCK_KEY = 7_214_609_311
TERMINAL_HARDWARE_STATUSES = (
    "completed",
    "failed",
    "cancelled",
    "rejected",
    "aborted",
)


@dataclass(frozen=True)
class RetentionRunResult:
    processed_jobs: int = 0
    diagnostics_acknowledgements: int = 0
    hardware_command_intents: int = 0
    dry_run: bool = False

    @property
    def total(self) -> int:
        return (
            self.processed_jobs
            + self.diagnostics_acknowledgements
            + self.hardware_command_intents
        )


async def run_database_retention_once(
    db: AsyncSession,
    *,
    now: datetime | None = None,
    dry_run: bool | None = None,
) -> RetentionRunResult:
    """Delete only expired operational records.

    Test evidence, signal revisions, allocations, reports, and current-state
    tables are deliberately outside this first retention policy.
    """

    settings = get_settings()
    current_time = now or datetime.now(timezone.utc)
    is_dry_run = settings.database_retention_dry_run if dry_run is None else dry_run
    batch_size = max(1, settings.database_retention_batch_size)

    if not await _try_acquire_lock(db):
        logger.debug("Database retention skipped because another instance owns the lock")
        return RetentionRunResult(dry_run=is_dry_run)

    try:
        result = RetentionRunResult(
            processed_jobs=await _purge_table(
                db,
                model=ProcessedJob,
                timestamp_column=ProcessedJob.processed_at,
                cutoff=current_time - timedelta(days=max(1, settings.database_processed_job_retention_days)),
                primary_columns=(ProcessedJob.worker_name, ProcessedJob.job_id),
                batch_size=batch_size,
                dry_run=is_dry_run,
            ),
            diagnostics_acknowledgements=await _purge_table(
                db,
                model=CoreDiagnosticsAcknowledgement,
                timestamp_column=CoreDiagnosticsAcknowledgement.acknowledged_at,
                cutoff=current_time - timedelta(days=max(1, settings.database_diagnostics_ack_retention_days)),
                primary_columns=(CoreDiagnosticsAcknowledgement.id,),
                batch_size=batch_size,
                dry_run=is_dry_run,
            ),
            hardware_command_intents=await _purge_hardware_commands(
                db,
                cutoff=current_time - timedelta(days=max(1, settings.database_hardware_command_retention_days)),
                batch_size=batch_size,
                dry_run=is_dry_run,
            ),
            dry_run=is_dry_run,
        )
        if not is_dry_run:
            await db.commit()
        return result
    except Exception:
        await db.rollback()
        raise
    finally:
        await _release_lock(db)


async def run_database_retention_worker() -> None:
    settings = get_settings()
    if not settings.database_retention_enabled:
        logger.info("Database retention worker disabled")
        return

    interval = max(60, settings.database_retention_interval_seconds)
    logger.info(
        "Database retention worker started (interval=%ss, dry_run=%s)",
        interval,
        settings.database_retention_dry_run,
    )
    while True:
        try:
            await asyncio.sleep(interval)
            async with AsyncSessionLocal() as db:
                result = await run_database_retention_once(db)
            if result.total:
                logger.info(
                    "Database retention completed: processed_jobs=%s diagnostics_acknowledgements=%s hardware_command_intents=%s dry_run=%s",
                    result.processed_jobs,
                    result.diagnostics_acknowledgements,
                    result.hardware_command_intents,
                    result.dry_run,
                )
        except asyncio.CancelledError:
            raise
        except SQLAlchemyError:
            logger.exception("Database retention failed due to a database error")
        except Exception:
            logger.exception("Database retention failed unexpectedly")


async def _purge_table(
    db: AsyncSession,
    *,
    model: type[Any],
    timestamp_column: Any,
    cutoff: datetime,
    primary_columns: tuple[Any, ...],
    batch_size: int,
    dry_run: bool,
) -> int:
    filters = timestamp_column < cutoff  # type: ignore[operator]
    if dry_run:
        value = await db.scalar(select(func.count()).select_from(model).where(filters))
        return int(value or 0)

    total = 0
    while True:
        rows: list[Any] = list((
            await db.execute(
                select(*primary_columns)
                .select_from(model)
                .where(filters)
                .order_by(*primary_columns)
                .limit(batch_size)
            )
        ).all())
        if not rows:
            return total

        if len(primary_columns) == 1:
            result = await db.execute(
                delete(model).where(primary_columns[0].in_([row[0] for row in rows]))  # type: ignore[union-attr]
            )
        else:
            result = await db.execute(
                delete(model).where(tuple_(*primary_columns).in_(rows))
            )
        total += int(getattr(result, "rowcount", 0) or 0)


async def _purge_hardware_commands(
    db: AsyncSession,
    *,
    cutoff: datetime,
    batch_size: int,
    dry_run: bool,
) -> int:
    filters = (
        HardwareCommandIntent.created_at < cutoff,
        HardwareCommandIntent.status.in_(TERMINAL_HARDWARE_STATUSES),
    )
    if dry_run:
        value = await db.scalar(
            select(func.count()).select_from(HardwareCommandIntent).where(*filters)
        )
        return int(value or 0)

    total = 0
    while True:
        ids = (
            await db.execute(
                select(HardwareCommandIntent.command_id)
                .where(*filters)
                .order_by(HardwareCommandIntent.created_at, HardwareCommandIntent.command_id)
                .limit(batch_size)
            )
        ).scalars().all()
        if not ids:
            return total
        result = await db.execute(
            delete(HardwareCommandIntent).where(HardwareCommandIntent.command_id.in_(ids))
        )
        total += int(getattr(result, "rowcount", 0) or 0)


async def _try_acquire_lock(db: AsyncSession) -> bool:
    value = await db.scalar(
        text("SELECT pg_try_advisory_lock(:lock_key)"),
        {"lock_key": RETENTION_ADVISORY_LOCK_KEY},
    )
    return bool(cast(bool, value))


async def _release_lock(db: AsyncSession) -> None:
    await db.scalar(
        text("SELECT pg_advisory_unlock(:lock_key)"),
        {"lock_key": RETENTION_ADVISORY_LOCK_KEY},
    )
