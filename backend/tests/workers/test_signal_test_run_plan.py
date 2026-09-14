from __future__ import annotations

from types import SimpleNamespace

import pytest

from app.workers.signal_test_run_runner import _load_immutable_plan_rows


class _FakeDb:
    def __init__(self, plan) -> None:
        self.plan = plan

    async def scalar(self, _statement):
        return self.plan


@pytest.mark.anyio
async def test_worker_loads_rows_from_immutable_plan_order() -> None:
    row = {
        "row_id": "signal-2",
        "signal_id": 2,
        "signal_key": "S2",
        "signal_name": "Signal 2",
        "signal_direction": "DO",
        "signal_metadata": {},
        "allocation_id": 20,
        "allocation_status": "assigned",
        "channel_id": 200,
        "channel_index": 1,
        "channel_type": "DO",
        "device_id": 30,
        "unit_id": "UNIT-1",
    }
    plan = SimpleNamespace(
        items=[
            SimpleNamespace(order_index=1, snapshot={**row, "signal_id": 3, "row_id": "signal-3", "signal_key": "S3"}),
            SimpleNamespace(order_index=0, snapshot=row),
        ]
    )
    repo = SimpleNamespace(db=_FakeDb(plan))

    rows = await _load_immutable_plan_rows(repo, 7, "job-1")

    assert [item.signal_id for item in rows] == [2, 3]
    assert rows[0].channel_id == 200


@pytest.mark.anyio
async def test_worker_rejects_missing_immutable_plan() -> None:
    repo = SimpleNamespace(db=_FakeDb(None))

    with pytest.raises(RuntimeError, match="Immutable test-run plan is missing"):
        await _load_immutable_plan_rows(repo, 7, "job-1")
