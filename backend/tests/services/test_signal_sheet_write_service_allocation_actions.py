from __future__ import annotations

import asyncio
from collections.abc import Awaitable
from dataclasses import dataclass
from typing import TypeVar, cast

import pytest
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.signal_sheet import SignalSheetAutoAllocateResult
from app.services.signal_sheet_write_service import SignalSheetWriteService
from app.api.v1.signal_sheet.repository import SignalSheetRepository


@dataclass
class FakeAllocation:
    channel_id: int
    signal_id: int = 2


class FakeDb:
    def __init__(self) -> None:
        self.commits: int = 0
        self.rollbacks: int = 0

    async def commit(self) -> None:
        self.commits += 1

    async def rollback(self) -> None:
        self.rollbacks += 1


class FakeRepo:
    def __init__(self, allocation: FakeAllocation | None = None) -> None:
        self.allocation: FakeAllocation | None = allocation
        self.channel_allocation: FakeAllocation = FakeAllocation(channel_id=11, signal_id=99)
        self.updated_entries: list[dict[str, object]] = []
        self.swapped: tuple[int, int, int] | None = None
        self.events: list[dict[str, object]] = []
        self.auto_allocate_result: SignalSheetAutoAllocateResult = SignalSheetAutoAllocateResult(
            requested=3,
            assigned=2,
            skipped=1,
            missing=0,
            unassigned_signal_ids=[],
            changed_signal_ids=[2, 3],
        )

    async def get_allocation_by_signal_id(self, workspace_id: int, signal_id: int) -> FakeAllocation | None:
        _ = (workspace_id, signal_id)
        return self.allocation

    async def get_allocation_by_channel_id(self, workspace_id: int, channel_id: int) -> FakeAllocation | None:
        _ = (workspace_id, channel_id)
        return self.channel_allocation

    async def update_allocations(
        self,
        workspace_id: int,
        entries: list[dict[str, object]],
        *,
        commit: bool,
    ) -> list[int]:
        _ = (workspace_id, commit)
        self.updated_entries.extend(entries)
        return [int(cast(int, item["signal_id"])) for item in entries]

    async def swap_allocations(
        self,
        *,
        workspace_id: int,
        signal_id: int,
        channel_id: int,
        commit: bool,
    ) -> list[int]:
        _ = commit
        self.swapped = (workspace_id, signal_id, channel_id)
        return [signal_id, 99]

    async def auto_allocate(self, **kwargs: object) -> SignalSheetAutoAllocateResult:
        _ = kwargs
        return self.auto_allocate_result

    async def record_allocation_event(self, **kwargs: object) -> None:
        self.events.append(dict(kwargs))


T = TypeVar("T")


def run_async(awaitable: Awaitable[T]) -> T:
    return asyncio.run(awaitable)


def _service(db: FakeDb, repo: FakeRepo) -> SignalSheetWriteService:
    return SignalSheetWriteService(
        db=cast(AsyncSession, cast(object, db)),
        repo=cast(SignalSheetRepository, cast(object, repo)),
    )


def test_assign_allocation_rejects_already_allocated_signal() -> None:
    db = FakeDb()
    repo = FakeRepo(allocation=FakeAllocation(channel_id=10))
    service = _service(db, repo)

    with pytest.raises(ValueError, match="already allocated"):
        _ = run_async(service.assign_allocation(workspace_id=1, signal_id=2, channel_id=11))

    assert db.rollbacks == 1
    assert repo.updated_entries == []
    assert repo.events == []


def test_assign_allocation_updates_unassigned_signal() -> None:
    db = FakeDb()
    repo = FakeRepo()
    service = _service(db, repo)

    changed = run_async(service.assign_allocation(workspace_id=1, signal_id=2, channel_id=11))

    assert changed == [2]
    assert db.commits == 1
    assert repo.updated_entries == [{"signal_id": 2, "channel_id": 11, "allocation_meta": None}]
    assert repo.events == [
        {
            "workspace_id": 1,
            "operation": "assign",
            "source": "api",
            "signal_id": 2,
            "previous_channel_id": None,
            "channel_id": 11,
            "requested_count": 1,
            "changed_count": 1,
            "payload": {"changed_signal_ids": [2]},
        }
    ]


def test_reassign_allocation_requires_existing_allocation() -> None:
    db = FakeDb()
    repo = FakeRepo()
    service = _service(db, repo)

    with pytest.raises(ValueError, match="not allocated"):
        _ = run_async(service.reassign_allocation(workspace_id=1, signal_id=2, channel_id=11))

    assert db.rollbacks == 1
    assert repo.updated_entries == []
    assert repo.events == []


def test_unassign_allocation_uses_explicit_null_channel() -> None:
    db = FakeDb()
    repo = FakeRepo(allocation=FakeAllocation(channel_id=10))
    service = _service(db, repo)

    changed = run_async(service.unassign_allocation(workspace_id=1, signal_id=2))

    assert changed == [2]
    assert db.commits == 1
    assert repo.updated_entries == [{"signal_id": 2, "channel_id": None}]
    assert repo.events[0]["operation"] == "unassign"
    assert repo.events[0]["previous_channel_id"] == 10
    assert repo.events[0]["channel_id"] is None


def test_swap_allocations_delegates_to_repo_transaction() -> None:
    db = FakeDb()
    repo = FakeRepo()
    service = _service(db, repo)

    changed = run_async(service.swap_allocations(workspace_id=1, signal_id=2, channel_id=11))

    assert changed == [2, 99]
    assert db.commits == 1
    assert repo.swapped == (1, 2, 11)
    assert repo.events[0]["operation"] == "swap"
    payload = cast(dict[str, object], repo.events[0]["payload"])
    assert payload["target_signal_id"] == 99


def test_bulk_update_records_summary_event() -> None:
    db = FakeDb()
    repo = FakeRepo()
    service = _service(db, repo)

    run_async(service.update_allocations(
        workspace_id=1,
        entries=[
            {"signal_id": 2, "channel_id": 11},
            {"signal_id": 3, "channel_id": None},
        ],
    ))

    assert db.commits == 1
    assert repo.events == [
        {
            "workspace_id": 1,
            "operation": "bulk_update",
            "source": "api",
            "requested_count": 2,
            "changed_count": 2,
            "skipped_count": 0,
            "rejected_count": 0,
            "payload": {
                "entry_count": 2,
                "changed_signal_ids": [2, 3],
            },
        }
    ]


def test_auto_allocate_records_summary_event() -> None:
    db = FakeDb()
    repo = FakeRepo()
    service = _service(db, repo)

    result = run_async(service.auto_allocate(
        workspace_id=1,
        signal_ids=[2, 3, 4],
        prefer_online=True,
        prefer_single_unit=False,
        overwrite_existing=False,
    ))

    assert result.changed_signal_ids == [2, 3]
    assert db.commits == 1
    assert repo.events[0]["operation"] == "auto_allocate"
    assert repo.events[0]["requested_count"] == 3
    assert repo.events[0]["changed_count"] == 2
    assert repo.events[0]["skipped_count"] == 1
    assert repo.events[0]["rejected_count"] == 0
