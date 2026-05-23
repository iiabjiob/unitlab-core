from __future__ import annotations

from datetime import datetime, timezone

from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.schemas.ws.events import SignalTestRuntimePatchEvent, WSChannel
from app.workers.signal_allocation_runner import _serialize_allocation_job_rows


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
