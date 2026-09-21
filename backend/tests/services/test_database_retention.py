from __future__ import annotations

import asyncio
from datetime import datetime, timezone
from types import SimpleNamespace

import pytest

from app.services import database_retention


class FakeSession:
    def __init__(self) -> None:
        self.scalar_calls = 0
        self.commit_calls = 0
        self.rollback_calls = 0
        self.execute_calls = 0

    async def scalar(self, _statement, _params=None):  # type: ignore[no-untyped-def]
        self.scalar_calls += 1
        return (4, 2, 3)[self.scalar_calls - 1]

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
    )
    monkeypatch.setattr(database_retention, "get_settings", lambda: settings)
    monkeypatch.setattr(database_retention, "_try_acquire_lock", lambda _db: _acquired())
    monkeypatch.setattr(database_retention, "_release_lock", lambda _db: _released())

    session = FakeSession()
    result = asyncio.run(database_retention.run_database_retention_once(
        session,  # type: ignore[arg-type]
        now=datetime(2026, 9, 21, tzinfo=timezone.utc),
        dry_run=True,
    ))

    assert result.processed_jobs == 4
    assert result.diagnostics_acknowledgements == 2
    assert result.hardware_command_intents == 3
    assert result.total == 9
    assert result.dry_run is True
    assert session.execute_calls == 0
    assert session.commit_calls == 0
    assert session.rollback_calls == 0


async def _acquired() -> bool:
    return True


async def _released() -> None:
    return None
