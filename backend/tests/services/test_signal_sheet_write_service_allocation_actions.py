from __future__ import annotations

import asyncio
from dataclasses import dataclass
from typing import Any

import pytest

from app.api.v1.signal_sheet import SignalSheetAutoAllocateResult
from app.services.signal_sheet_write_service import SignalSheetWriteService


@dataclass
class FakeAllocation:
    channel_id: int
    signal_id: int = 2


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
        self.channel_allocation = FakeAllocation(channel_id=11, signal_id=99)
        self.updated_entries: list[dict[str, Any]] = []
        self.swapped: tuple[int, int, int] | None = None
        self.events: list[dict[str, Any]] = []
        self.auto_allocate_result = SignalSheetAutoAllocateResult(
            requested=3,
            assigned=2,
            skipped=1,
            missing=0,
            unassigned_signal_ids=[],
            changed_signal_ids=[2, 3],
        )

    async def get_allocation_by_signal_id(self, workspace_id: int, signal_id: int) -> FakeAllocation | None:
        return self.allocation

    async def get_allocation_by_channel_id(self, workspace_id: int, channel_id: int) -> FakeAllocation | None:
        return self.channel_allocation

    async def update_allocations(
        self,
        workspace_id: int,
        entries: list[dict[str, Any]],
        *,
        commit: bool,
    ) -> list[int]:
        self.updated_entries.extend(entries)
        return [int(item["signal_id"]) for item in entries]

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

    async def auto_allocate(self, **kwargs) -> SignalSheetAutoAllocateResult:
        return self.auto_allocate_result

    async def record_allocation_event(self, **kwargs) -> None:
        self.events.append(dict(kwargs))


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
    assert repo.events == []


def test_assign_allocation_updates_unassigned_signal() -> None:
    db = FakeDb()
    repo = FakeRepo()
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

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
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

    with pytest.raises(ValueError, match="not allocated"):
        run_async(service.reassign_allocation(workspace_id=1, signal_id=2, channel_id=11))

    assert db.rollbacks == 1
    assert repo.updated_entries == []
    assert repo.events == []


def test_unassign_allocation_uses_explicit_null_channel() -> None:
    db = FakeDb()
    repo = FakeRepo(allocation=FakeAllocation(channel_id=10))
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

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
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

    changed = run_async(service.swap_allocations(workspace_id=1, signal_id=2, channel_id=11))

    assert changed == [2, 99]
    assert db.commits == 1
    assert repo.swapped == (1, 2, 11)
    assert repo.events[0]["operation"] == "swap"
    assert repo.events[0]["payload"]["target_signal_id"] == 99


def test_bulk_update_records_summary_event() -> None:
    db = FakeDb()
    repo = FakeRepo()
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

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
    service = SignalSheetWriteService(db=db, repo=repo)  # type: ignore[arg-type]

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
