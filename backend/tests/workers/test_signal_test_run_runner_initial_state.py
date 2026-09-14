from __future__ import annotations

import asyncio
from types import SimpleNamespace

from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.workers import signal_test_run_runner


class FakeDb:
    async def commit(self) -> None:
        return None

    async def rollback(self) -> None:
        return None


class MissingBitmaskRedis:
    async def get(self, key: str):
        return None

    async def set(self, key: str, value, **kwargs) -> None:
        return None

    async def expire(self, key: str, ttl: int) -> None:
        return None


class Repo:
    def __init__(self, row: SignalAllocationRowSchema) -> None:
        self.db = FakeDb()
        self.row = row
        self.evidence: list[dict] = []

    async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
        return [self.row] if int(self.row.signal_id) in {int(signal_id) for signal_id in signal_ids} else []

    async def record_signal_test_run_step_evidence(self, **kwargs) -> None:
        self.evidence.append(dict(kwargs))


def run_async(awaitable):
    return asyncio.run(awaitable)


def test_missing_initial_do_state_skips_before_admission_and_publish(monkeypatch) -> None:
    row = SignalAllocationRowSchema(
        row_id="signal-1",
        signal_id=1,
        signal_key="S1",
        signal_name="Signal 1",
        signal_direction="DO",
        allocation_status="assigned",
        allocation_health={"offline_device": False},
        channel_id=101,
        device_id=201,
        channel_type="do",
        channel_index=0,
        channel_label="unit-1/DO0",
        unit_id="unit-1",
        unit_online=True,
    )
    repo = Repo(row)
    admission_calls: list[dict] = []
    publish_calls: list[object] = []
    command_calls: list[dict] = []

    class Admission:
        def __init__(self, redis) -> None:
            return None

        async def acquire(self, **kwargs):
            admission_calls.append(kwargs)
            return SimpleNamespace(lease_id="unexpected")

    async def load_plan(repo, workspace_id: int, job_id: str):
        return [repo.row]

    async def publish(event) -> None:
        publish_calls.append(event)

    async def enqueue_command(**kwargs) -> None:
        command_calls.append(kwargs)

    async def no_recovery(*args, **kwargs) -> bool:
        return False

    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: MissingBitmaskRedis())
    monkeypatch.setattr(signal_test_run_runner, "_load_immutable_plan_rows", load_plan)
    monkeypatch.setattr(signal_test_run_runner, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish)
    monkeypatch.setattr(signal_test_run_runner, "has_hardware_recovery_required", no_recovery)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_do_command", enqueue_command)

    result = run_async(
        signal_test_run_runner._handle_test_run(
            repo,
            7,
            {
                "job_id": "job-1",
                "signal_ids": [1],
                "signal_interval_ms": 100,
                "toggle_mode": "single",
            },
            {
                "job_id": "job-1",
                "workspace_id": 7,
                "operation": "test_run",
                "status": "queued",
                "created_at": "2026-01-01T12:30:00+00:00",
            },
        )
    )

    assert result["succeeded"] == 0
    assert result["skipped"] == 1
    assert result["skip_reasons"]["initial_state_unknown"] == 1
    assert admission_calls == []
    assert command_calls == []
    assert repo.evidence[0]["reason"] == "initial_state_unknown"
