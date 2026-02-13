from __future__ import annotations

import csv
import io

from app.models.test_run import TestRun
from app.services.signal_binding_service import SignalBindingService


class CableJournalService:
    """Render allocation details as CSV for field wiring operations."""

    @staticmethod
    def export_csv(run: TestRun) -> str:
        output = io.StringIO()
        writer = csv.writer(output)
        writer.writerow(
            [
                "test_run_id",
                "allocation_revision",
                "allocation_entry_id",
                "signal_key",
                "signal_name",
                "signal_direction",
                "channel_id",
                "channel_type",
                "unit_id",
                "terminal_label",
            ]
        )

        entries = run.allocation.entries if run.allocation else []
        for entry in entries:
            signal_key = SignalBindingService.extract_signal_key(entry)
            signal_name = entry.signal.name if entry.signal is not None else ""
            signal_direction = ""
            if entry.signal is not None:
                direction_value = entry.signal.io_direction
                signal_direction = direction_value if isinstance(direction_value, str) else direction_value.value

            channel = entry.channel
            unit_id = channel.device.unit_id if channel and channel.device else ""
            channel_index = channel.channel_index + 1 if channel else ""
            terminal_label = f"{unit_id}:CH{channel_index}" if channel else ""

            writer.writerow(
                [
                    run.id,
                    run.allocation_revision,
                    entry.id,
                    signal_key or "",
                    signal_name,
                    signal_direction,
                    entry.channel_id,
                    channel.channel_type if channel else "",
                    unit_id,
                    terminal_label,
                ]
            )

        return output.getvalue()
