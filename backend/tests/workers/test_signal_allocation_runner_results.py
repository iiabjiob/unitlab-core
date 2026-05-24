from __future__ import annotations

import asyncio
from datetime import datetime, timezone

from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.schemas.ws.events import SignalTestRuntimePatchEvent, WSChannel
from app.workers import signal_test_run_runner
from app.workers.signal_allocation_runner import _serialize_allocation_job_rows


def run_async(awaitable):
    return asyncio.run(awaitable)


class FakeNoRowsRepo:
    async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
        return []


class FakeRepoDb:
    async def commit(self) -> None:
        return None

    async def rollback(self) -> None:
        return None


class FakeLiveRowsRepo:
    def __init__(self) -> None:
        self.db = FakeRepoDb()
        self.calls: list[list[int]] = []
        self.tested_at_by_signal: dict[int, str] = {}

    async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
        normalized_ids = [int(signal_id) for signal_id in signal_ids]
        self.calls.append(normalized_ids)
        if len(normalized_ids) != 1:
            raise AssertionError("test-run worker must resolve one current binding at execution time")
        signal_id = normalized_ids[0]
        return [build_allocation_row(signal_id)]

    async def mark_signals_tested_at(
        self,
        workspace_id: int,
        tested_at_by_signal: dict[int, str],
        *,
        commit: bool,
    ) -> list[int]:
        self.tested_at_by_signal.update(tested_at_by_signal)
        return sorted(tested_at_by_signal)


class FakeRedis:
    async def get(self, key: str):
        return "0"


def build_allocation_row(signal_id: int, **overrides) -> SignalAllocationRowSchema:
    data = {
        "row_id": f"signal-{signal_id}",
        "signal_id": signal_id,
        "signal_key": f"S{signal_id}",
        "signal_name": f"Signal {signal_id}",
        "signal_direction": "DI",
        "allocation_status": "assigned",
        "allocation_health": {"offline_device": False},
        "channel_id": 100 + signal_id,
        "channel_type": "do",
        "channel_index": signal_id,
        "channel_label": f"unit-{signal_id}/DO{signal_id}",
        "unit_id": f"unit-{signal_id}",
        "unit_online": True,
    }
    data.update(overrides)
    return SignalAllocationRowSchema(**data)


def test_serialize_allocation_job_rows_returns_json_safe_projection_rows() -> None:
    rows = [
        SignalAllocationRowSchema(
            row_id="signal-1",
            signal_id=1,
            signal_key="S1",
            signal_name="Signal 1",
            signal_direction="DI",
            allocation_status="assigned",
            allocation_health={"offline_device": False},
            channel_id=10,
            tested_at=datetime(2026, 1, 1, 12, 30, tzinfo=timezone.utc),
        )
    ]

    payload = _serialize_allocation_job_rows(rows)

    assert payload[0]["row_id"] == "signal-1"
    assert payload[0]["signal_id"] == 1
    assert payload[0]["allocation_status"] == "assigned"
    assert payload[0]["allocation_health"] == {"offline_device": False}
    assert isinstance(payload[0]["tested_at"], str)
    assert payload[0]["tested_at"].startswith("2026-01-01T12:30:00")


def test_signal_test_runtime_patch_event_serializes_tested_at_by_signal() -> None:
    event = SignalTestRuntimePatchEvent(
        job_id="job-1",
        workspace_id=7,
        tested_at_by_signal={1: "2026-01-01T12:30:00+00:00"},
        emitted_at=datetime(2026, 1, 1, 12, 30, tzinfo=timezone.utc),
    )

    payload = event.model_dump(mode="json")

    assert payload["channel"] == WSChannel.SYSTEM_INFO.value
    assert payload["event"] == "signal_test_runtime_patch"
    assert payload["patch_type"] == "tested_at"
    assert payload["tested_at_by_signal"] == {"1": "2026-01-01T12:30:00+00:00"}
    assert payload["emitted_at"].startswith("2026-01-01T12:30:00")


def test_signal_test_run_ignores_stale_sheet_metadata_and_skips_missing_signal(monkeypatch) -> None:
    async def publish_noop(event) -> None:
        return None

    payload = {
        "signal_ids": [1],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
        "signal_sheet_revision": {"revision_token": "queued"},
    }
    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: object())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_noop)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }

    result = run_async(
        signal_test_run_runner._handle_test_run(FakeNoRowsRepo(), 7, payload, job_state)  # type: ignore[arg-type]
    )

    assert result["processed"] == 1
    assert result["succeeded"] == 0
    assert result["skipped"] == 1
    assert result["skip_reasons"]["missing_row"] == 1
    assert "signal_sheet_revision" not in result


def test_signal_test_run_resolves_current_binding_per_signal(monkeypatch) -> None:
    repo = FakeLiveRowsRepo()
    commands: list[tuple[str, dict]] = []

    async def publish_noop(event) -> None:
        return None

    async def sleep_noop(seconds: float) -> None:
        return None

    async def enqueue_do_noop(**kwargs) -> None:
        commands.append(("do", dict(kwargs)))

    async def enqueue_state_noop(**kwargs) -> None:
        commands.append(("state", dict(kwargs)))

    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: FakeRedis())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_noop)
    monkeypatch.setattr(signal_test_run_runner.asyncio, "sleep", sleep_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_do_command", enqueue_do_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_request_state", enqueue_state_noop)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }
    payload = {
        "signal_ids": [1, 2],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
    }

    result = run_async(
        signal_test_run_runner._handle_test_run(repo, 7, payload, job_state)  # type: ignore[arg-type]
    )

    assert repo.calls == [[1], [2]]
    assert [(kind, item["unit_id"], item["ch"]) for kind, item in commands if kind == "do"] == [
        ("do", "unit-1", 1),
        ("do", "unit-2", 2),
    ]
    assert result["succeeded"] == 2
    assert result["skipped"] == 0
    assert sorted(repo.tested_at_by_signal) == [1, 2]


def test_signal_test_run_skips_non_executable_current_bindings(monkeypatch) -> None:
    rows_by_signal_id = {
        1: build_allocation_row(1, unit_id=None),
        2: build_allocation_row(2, channel_type="di"),
        3: build_allocation_row(3, unit_online=False),
    }
    commands: list[dict] = []

    class FakeRowsRepo:
        async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
            signal_id = int(signal_ids[0])
            return [rows_by_signal_id[signal_id]]

    async def publish_noop(event) -> None:
        return None

    async def enqueue_do_noop(**kwargs) -> None:
        commands.append(dict(kwargs))

    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: FakeRedis())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_do_command", enqueue_do_noop)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }
    payload = {
        "signal_ids": [1, 2, 3],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
    }

    result = run_async(
        signal_test_run_runner._handle_test_run(FakeRowsRepo(), 7, payload, job_state)  # type: ignore[arg-type]
    )

    assert result["succeeded"] == 0
    assert result["skipped"] == 3
    assert result["skip_reasons"]["invalid_binding"] == 1
    assert result["skip_reasons"]["incompatible_channel_mode"] == 1
    assert result["skip_reasons"]["offline_unit"] == 1
    assert commands == []
