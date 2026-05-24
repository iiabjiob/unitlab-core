from __future__ import annotations

import asyncio
from dataclasses import dataclass
from typing import Any

import pytest

from app.services.signal_sheet_write_service import SignalSheetWriteService


@dataclass
class FakeAllocation:
    channel_id: int


class FakeDb:
    def __init__(self) -> None:
        self.commits = 0
        self.rollbacks = 0

    async def commit(self) -> None:
        self.commits += 1

    async def rollback(self) -> None:
        self.rollbacks += 1


class FakeRepo:
    def __init__(self, allocation: FakeAllocation | None = None) -> None:
        self.allocation = allocation
        self.updated_entries: list[dict[str, Any]] = []
        self.swapped: tuple[int, int, int] | None = None

    async def get_allocation_by_signal_id(self, workspace_id: int, signal_id: int) -> FakeAllocation | None:
        return self.allocation

    async def update_allocations(
        self,
        workspace_id: int,
        entries: list[dict[str, Any]],
        *,
        commit: bool,
    ) -> None:
        self.updated_entries.extend(entries)

    async def swap_allocations(
        self,
        *,
        workspace_id: int,
        signal_id: int,
        channel_id: int,
        commit: bool,
    ) -> list[int]:
        self.swapped = (workspace_id, signal_id, channel_id)
        return [signal_id, 99]

def run_async[T](awaitable: Any) -> T:
    return asyncio.run(awaitable)


def test_assign_allocation_rejects_already_allocated_signal() -> None:
    db = FakeDb()
    repo = FakeRepo(allocation=FakeAllocation(channel_id=10))
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

    with pytest.raises(ValueError, match="already allocated"):
        run_async(service.assign_allocation(workspace_id=1, signal_id=2, channel_id=11))

    assert db.rollbacks == 1
    assert repo.updated_entries == []


def test_assign_allocation_updates_unassigned_signal() -> None:
    db = FakeDb()
    repo = FakeRepo()
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

    changed = run_async(service.assign_allocation(workspace_id=1, signal_id=2, channel_id=11))

    assert changed == [2]
    assert db.commits == 1
    assert repo.updated_entries == [{"signal_id": 2, "channel_id": 11, "allocation_meta": None}]


def test_reassign_allocation_requires_existing_allocation() -> None:
    db = FakeDb()
    repo = FakeRepo()
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

    with pytest.raises(ValueError, match="not allocated"):
        run_async(service.reassign_allocation(workspace_id=1, signal_id=2, channel_id=11))

    assert db.rollbacks == 1
    assert repo.updated_entries == []


def test_unassign_allocation_uses_explicit_null_channel() -> None:
    db = FakeDb()
    repo = FakeRepo(allocation=FakeAllocation(channel_id=10))
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

    changed = run_async(service.unassign_allocation(workspace_id=1, signal_id=2))

    assert changed == [2]
    assert db.commits == 1
    assert repo.updated_entries == [{"signal_id": 2, "channel_id": None}]


def test_swap_allocations_delegates_to_repo_transaction() -> None:
    db = FakeDb()
    repo = FakeRepo()
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

    changed = run_async(service.swap_allocations(workspace_id=1, signal_id=2, channel_id=11))

    assert changed == [2, 99]
    assert db.commits == 1
    assert repo.swapped == (1, 2, 11)
