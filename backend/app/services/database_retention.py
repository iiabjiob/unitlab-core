from __future__ import annotations

import asyncio
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from typing import Any, cast

from sqlalchemy import delete, desc, exists, func, select, text, tuple_
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.models.core_diagnostics import CoreDiagnosticsAcknowledgement
from app.models.hardware_command import HardwareCommandIntent
from app.models.processed_job import ProcessedJob
from app.models.signal_revision import SignalListRevision, SignalTestRunPlan
from app.models.signal_sheet import SignalAllocationEvent, SignalTestRunStepEvidence
from app.models.sequence_run import SequenceRun, SequenceRunStatus
from app.models.verification_evidence import SignalVerificationEvidence, SignalVerificationEvidenceSet
from app.models.verification_run import SignalVerificationRun
from app.models.workspace_iec61850 import (
    WorkspaceIec61850RuntimeSelection,
    WorkspaceIec61850RuntimeSelectionEvent,
    WorkspaceIec61850SclImport,
)
from app.models.workspace_sld import WorkspaceSldDocumentRevision


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
    runtime_events: int = 0
    allocation_events: int = 0
    scl_imports: int = 0
    signal_revisions: int = 0
    sld_revisions: int = 0
    test_evidence: int = 0
    sequence_runs: int = 0
    dry_run: bool = False

    @property
    def total(self) -> int:
        return (
            self.processed_jobs
            + self.diagnostics_acknowledgements
            + self.hardware_command_intents
            + self.runtime_events
            + self.allocation_events
            + self.scl_imports
            + self.signal_revisions
            + self.sld_revisions
            + self.test_evidence
            + self.sequence_runs
        )


async def run_database_retention_once(
    db: AsyncSession,
    *,
    now: datetime | None = None,
    dry_run: bool | None = None,
) -> RetentionRunResult:
    """Delete only expired operational records.

    Test evidence, allocations, reports, and current-state tables are
    deliberately outside this retention policy.
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
            runtime_events=await _purge_table(
                db,
                model=WorkspaceIec61850RuntimeSelectionEvent,
                timestamp_column=WorkspaceIec61850RuntimeSelectionEvent.created_at,
                cutoff=current_time - timedelta(days=max(1, settings.database_runtime_event_retention_days)),
                primary_columns=(WorkspaceIec61850RuntimeSelectionEvent.id,),
                batch_size=batch_size,
                dry_run=is_dry_run,
            ),
            allocation_events=await _purge_table(
                db,
                model=SignalAllocationEvent,
                timestamp_column=SignalAllocationEvent.created_at,
                cutoff=current_time - timedelta(days=max(1, settings.database_allocation_event_retention_days)),
                primary_columns=(SignalAllocationEvent.id,),
                batch_size=batch_size,
                dry_run=is_dry_run,
            ),
            scl_imports=await _purge_scl_imports(
                db,
                cutoff=current_time - timedelta(days=max(1, settings.database_scl_import_retention_days)),
                batch_size=batch_size,
                dry_run=is_dry_run,
            ),
            signal_revisions=await _purge_signal_revisions(
                db,
                cutoff=current_time - timedelta(days=max(1, settings.database_signal_revision_retention_days)),
                batch_size=batch_size,
                dry_run=is_dry_run,
            ),
            sld_revisions=await _purge_sld_revisions(
                db,
                cutoff=current_time - timedelta(days=max(1, settings.database_sld_revision_retention_days)),
                keep_count=max(1, settings.database_sld_revision_keep_count),
                batch_size=batch_size,
                dry_run=is_dry_run,
            ),
            test_evidence=await _purge_test_evidence(
                db,
                cutoff=current_time - timedelta(days=max(1, settings.database_test_evidence_retention_days)),
                batch_size=batch_size,
                dry_run=is_dry_run,
            ),
            sequence_runs=await _purge_sequence_runs(
                db,
                cutoff=current_time - timedelta(days=max(1, settings.database_test_evidence_retention_days)),
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
                await _log_database_size(db)
            if result.total:
                logger.info(
                    "Database retention completed: processed_jobs=%s diagnostics_acknowledgements=%s hardware_command_intents=%s runtime_events=%s allocation_events=%s scl_imports=%s signal_revisions=%s sld_revisions=%s test_evidence=%s sequence_runs=%s dry_run=%s",
                    result.processed_jobs,
                    result.diagnostics_acknowledgements,
                    result.hardware_command_intents,
                    result.runtime_events,
                    result.allocation_events,
                    result.scl_imports,
                    result.signal_revisions,
                    result.sld_revisions,
                    result.test_evidence,
                    result.sequence_runs,
                    result.dry_run,
                )
        except asyncio.CancelledError:
            raise
        except SQLAlchemyError:
            logger.exception("Database retention failed due to a database error")
        except Exception:
            logger.exception("Database retention failed unexpectedly")


async def _log_database_size(db: AsyncSession) -> None:
    settings = get_settings()
    size_bytes = await db.scalar(text("SELECT pg_database_size(current_database())"))
    if not isinstance(size_bytes, int):
        return
    size_gb = size_bytes / (1024 ** 3)
    if size_gb >= settings.database_size_warning_gb:
        logger.warning(
            "PostgreSQL database size is %.2f GiB, above configured warning threshold %.2f GiB",
            size_gb,
            settings.database_size_warning_gb,
        )
    else:
        logger.debug("PostgreSQL database size is %.2f GiB", size_gb)


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


async def _purge_scl_imports(
    db: AsyncSession,
    *,
    cutoff: datetime,
    batch_size: int,
    dry_run: bool,
) -> int:
    protected = (
        exists(
            select(1).where(
                WorkspaceIec61850RuntimeSelection.scl_import_id == WorkspaceIec61850SclImport.id
            )
        )
        | exists(
            select(1).where(
                WorkspaceIec61850RuntimeSelectionEvent.scl_import_id == WorkspaceIec61850SclImport.id
            )
        )
    )
    filters = (
        WorkspaceIec61850SclImport.created_at < cutoff,
        ~protected,
    )
    if dry_run:
        value = await db.scalar(select(func.count()).select_from(WorkspaceIec61850SclImport).where(*filters))
        return int(value or 0)
    return await _purge_ids(
        db,
        model=WorkspaceIec61850SclImport,
        id_column=WorkspaceIec61850SclImport.id,
        filters=filters,
        order_columns=(WorkspaceIec61850SclImport.created_at, WorkspaceIec61850SclImport.id),
        batch_size=batch_size,
    )


async def _purge_signal_revisions(
    db: AsyncSession,
    *,
    cutoff: datetime,
    batch_size: int,
    dry_run: bool,
) -> int:
    protected = (
        exists(select(1).where(SignalTestRunPlan.revision_id == SignalListRevision.id))
        | exists(select(1).where(SignalTestRunStepEvidence.signal_list_revision_id == SignalListRevision.id))
    )
    filters = (
        SignalListRevision.status != "active",
        SignalListRevision.created_at < cutoff,
        ~protected,
    )
    if dry_run:
        value = await db.scalar(select(func.count()).select_from(SignalListRevision).where(*filters))
        return int(value or 0)
    return await _purge_ids(
        db,
        model=SignalListRevision,
        id_column=SignalListRevision.id,
        filters=filters,
        order_columns=(SignalListRevision.created_at, SignalListRevision.id),
        batch_size=batch_size,
    )


async def _purge_sld_revisions(
    db: AsyncSession,
    *,
    cutoff: datetime,
    keep_count: int,
    batch_size: int,
    dry_run: bool,
) -> int:
    ranked_revisions = (
        select(
            WorkspaceSldDocumentRevision.id.label("id"),
            WorkspaceSldDocumentRevision.created_at.label("created_at"),
            func.row_number()
            .over(
                partition_by=WorkspaceSldDocumentRevision.document_id,
                order_by=desc(WorkspaceSldDocumentRevision.revision),
            )
            .label("revision_rank"),
        )
        .subquery()
    )
    candidates = (
        select(ranked_revisions.c.id)
        .where(ranked_revisions.c.created_at < cutoff)
        .where(ranked_revisions.c.revision_rank > keep_count)
    )
    if dry_run:
        value = await db.scalar(select(func.count()).select_from(candidates.subquery()))
        return int(value or 0)
    total = 0
    while True:
        ids = (await db.execute(candidates.limit(batch_size))).scalars().all()
        if not ids:
            return total
        result = await db.execute(
            delete(WorkspaceSldDocumentRevision).where(WorkspaceSldDocumentRevision.id.in_(ids))
        )
        total += int(getattr(result, "rowcount", 0) or 0)


async def _purge_ids(
    db: AsyncSession,
    *,
    model: type[Any],
    id_column: Any,
    filters: tuple[Any, ...],
    order_columns: tuple[Any, ...],
    batch_size: int,
) -> int:
    total = 0
    while True:
        ids = (
            await db.execute(
                select(id_column).where(*filters).order_by(*order_columns).limit(batch_size)
            )
        ).scalars().all()
        if not ids:
            return total
        result = await db.execute(delete(model).where(id_column.in_(ids)))
        total += int(getattr(result, "rowcount", 0) or 0)


async def _purge_test_evidence(
    db: AsyncSession,
    *,
    cutoff: datetime,
    batch_size: int,
    dry_run: bool,
) -> int:
    total = 0
    for model, timestamp_column, id_column in (
        (SignalTestRunStepEvidence, SignalTestRunStepEvidence.created_at, SignalTestRunStepEvidence.id),
        (SignalVerificationEvidence, SignalVerificationEvidence.created_at, SignalVerificationEvidence.id),
        (SignalVerificationEvidenceSet, SignalVerificationEvidenceSet.created_at, SignalVerificationEvidenceSet.id),
        (SignalVerificationRun, SignalVerificationRun.created_at, SignalVerificationRun.id),
    ):
        total += await _purge_table(
            db,
            model=model,
            timestamp_column=timestamp_column,
            cutoff=cutoff,
            primary_columns=(id_column,),
            batch_size=batch_size,
            dry_run=dry_run,
        )
    return total


async def _purge_sequence_runs(
    db: AsyncSession,
    *,
    cutoff: datetime,
    batch_size: int,
    dry_run: bool,
) -> int:
    terminal_statuses = tuple(
        status.value
        for status in (
            SequenceRunStatus.COMPLETED,
            SequenceRunStatus.COMPLETED_WITH_ISSUES,
            SequenceRunStatus.STOPPED,
            SequenceRunStatus.ERROR,
            SequenceRunStatus.BLOCKED,
        )
    )
    filters = (
        SequenceRun.finished_at.is_not(None),
        SequenceRun.finished_at < cutoff,
        SequenceRun.status.in_(terminal_statuses),
    )
    if dry_run:
        value = await db.scalar(select(func.count()).select_from(SequenceRun).where(*filters))
        return int(value or 0)
    return await _purge_ids(
        db,
        model=SequenceRun,
        id_column=SequenceRun.id,
        filters=filters,
        order_columns=(SequenceRun.finished_at, SequenceRun.id),
        batch_size=batch_size,
    )


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
