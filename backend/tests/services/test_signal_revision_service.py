from __future__ import annotations

import asyncio
from collections.abc import Awaitable
from types import SimpleNamespace
from typing import Protocol, TypeVar, cast

from sqlalchemy.ext.asyncio import AsyncSession

from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.services.signal_revision_service import SignalRevisionService

T = TypeVar("T")


class _Statement(Protocol):
    def compile(self, **kwargs: object) -> object: ...


def run_async(awaitable: Awaitable[T]) -> T:
    return asyncio.run(awaitable)


def test_create_active_revision_archives_previous_active_revision_first() -> None:
    calls: list[str] = []

    class FakeDb:
        async def execute(self, statement: _Statement) -> SimpleNamespace:
            calls.append("archive")
            assert "'archived'" in str(statement.compile(compile_kwargs={"literal_binds": True}))
            return SimpleNamespace()

        async def scalar(self, statement: _Statement) -> int:
            _ = statement
            calls.append("max")
            return 3

        def add(self, item: object) -> None:
            _ = item
            calls.append("add")

        async def flush(self) -> None:
            calls.append("flush")

    row = SignalAllocationRowSchema(
        row_id="signal-1",
        signal_id=1,
        signal_key="S1",
        signal_name="Signal 1",
        signal_direction="DI",
        channel_id=None,
        channel_index=None,
        channel_type=None,
        unit_id=None,
        device_id=None,
        allocation_status="unassigned",
    )

    revision = run_async(
        SignalRevisionService(cast(AsyncSession, cast(object, FakeDb()))).create_active_revision(
            workspace_id=7,
            rows=[row],
        )
    )

    assert revision.revision_no == 4
    assert revision.status == "active"
    assert calls == ["archive", "max", "add", "flush"]
