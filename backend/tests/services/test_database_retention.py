from __future__ import annotations

import asyncio
from datetime import datetime, timezone
from types import SimpleNamespace
from typing import cast

import pytest
from sqlalchemy.ext.asyncio import AsyncSession

from app.services import database_retention


class FakeSession:
    def __init__(self) -> None:
        self.scalar_calls = 0
        self.commit_calls = 0
        self.rollback_calls = 0
        self.execute_calls = 0
        self.scalar_values = iter((4, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14))

    async def scalar(self, _statement, _params=None):  # type: ignore[no-untyped-def]
        self.scalar_calls += 1
        return next(self.scalar_values)

    async def execute(self, _statement):  # type: ignore[no-untyped-def]
        self.execute_calls += 1
        raise AssertionError("dry-run retention must not execute delete batches")

    async def commit(self) -> None:
        self.commit_calls += 1

    async def rollback(self) -> None:
        self.rollback_calls += 1


def test_retention_dry_run_counts_only_configured_operational_tables(monkeypatch: pytest.MonkeyPatch) -> None:
    settings = SimpleNamespace(
        database_retention_dry_run=False,
        database_retention_batch_size=100,
        database_processed_job_retention_days=30,
        database_diagnostics_ack_retention_days=90,
        database_hardware_command_retention_days=365,
        database_runtime_event_retention_days=90,
        database_allocation_event_retention_days=730,
        database_scl_import_retention_days=180,
        database_signal_revision_retention_days=730,
        database_sld_revision_retention_days=180,
        database_sld_revision_keep_count=50,
        database_test_evidence_retention_days=2555,
        database_size_warning_gb=20.0,
    )
    monkeypatch.setattr(database_retention, "get_settings", lambda: settings)
    monkeypatch.setattr(database_retention, "_try_acquire_lock", lambda _db: _acquired())
    monkeypatch.setattr(database_retention, "_release_lock", lambda _db: _released())

    session = FakeSession()
    result = asyncio.run(database_retention.run_database_retention_once(
        cast(AsyncSession, cast(object, session)),
        now=datetime(2026, 9, 21, tzinfo=timezone.utc),
        dry_run=True,
    ))

    assert result.processed_jobs == 4
    assert result.diagnostics_acknowledgements == 2
    assert result.hardware_command_intents == 3
    assert result.runtime_events == 5
    assert result.allocation_events == 6
    assert result.scl_imports == 7
    assert result.signal_revisions == 8
    assert result.sld_revisions == 9
    assert result.test_evidence == 46
    assert result.sequence_runs == 14
    assert result.total == 104
    assert result.dry_run is True
    assert session.execute_calls == 0
    assert session.commit_calls == 0
    assert session.rollback_calls == 0


async def _acquired() -> bool:
    return True


async def _released() -> None:
    return None
