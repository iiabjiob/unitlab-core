from __future__ import annotations

from app.models.channel import Channel
from app.models.device import Device
from app.models.signal import Signal, SignalIODirection
from app.models.test_run import TestRun, TestRunAllocation, TestRunAllocationEntry
from app.services.cable_journal_service import CableJournalService


def test_export_csv_contains_signal_and_terminal_mapping() -> None:
    device = Device(id=1, unit_id="DO-RPI", last_seen_at=None)
    channel = Channel(id=10, device_id=1, channel_index=3, channel_type="DO")
    channel.device = device

    signal = Signal(
        id=42,
        workspace_id=1,
        key="breaker_close",
        name="Breaker Close",
        io_direction=SignalIODirection.DO,
        signal_metadata={},
    )

    entry = TestRunAllocationEntry(id=7, channel_id=10, signal_id=42)
    entry.channel = channel
    entry.signal = signal

    allocation = TestRunAllocation(id=3, test_run_id=2, notes="check")
    allocation.entries = [entry]

    run = TestRun(id=2, workspace_id=1, allocation_revision=4)
    run.allocation = allocation

    csv_payload = CableJournalService.export_csv(run)

    assert "test_run_id" in csv_payload
    assert "breaker_close" in csv_payload
    assert "DO-RPI:CH4" in csv_payload
    assert ",4," in csv_payload
