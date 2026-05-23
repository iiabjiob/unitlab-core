from __future__ import annotations

import asyncio
from datetime import datetime, timezone
from types import SimpleNamespace

import pytest

from app.api.v1.signal_sheet.router import _build_signal_test_run_job_payload
from app.schemas.signal_sheet_schema import SignalAllocationRowSchema, SignalTestRunJobSchema
from app.schemas.ws.events import SignalTestRuntimePatchEvent, WSChannel
from app.workers.signal_allocation_runner import _serialize_allocation_job_rows
from app.workers.signal_test_run_runner import _handle_test_run


def run_async(awaitable):
    return asyncio.run(awaitable)


class FakeRevisionRepo:
    async def get_sheet_revision_snapshot(self, workspace_id: int):
        return SimpleNamespace(revision_token="current")


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


def test_signal_test_run_rejects_stale_signal_sheet_revision() -> None:
    payload = {
        "signal_ids": [1],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
        "signal_sheet_revision": {"revision_token": "queued"},
    }

    with pytest.raises(ValueError, match="revision changed"):
        run_async(_handle_test_run(FakeRevisionRepo(), 7, payload, {}))  # type: ignore[arg-type]


def test_signal_test_run_job_payload_includes_queued_revision() -> None:
    request = SignalTestRunJobSchema(
        signal_ids=[1, 2],
        signal_interval_ms=100,
        toggle_mode="single",
    )
    revision = SimpleNamespace(to_payload=lambda: {"revision_token": "rev-1"})

    payload = _build_signal_test_run_job_payload(request, revision)

    assert payload["signal_ids"] == [1, 2]
    assert payload["signal_sheet_revision"] == {"revision_token": "rev-1"}
