from __future__ import annotations

from app.models.signal import Signal, SignalIODirection
from app.models.test_run import TestRunAllocationEntry
from app.services.signal_binding_service import SignalBindingService


def test_build_bindings_prefers_linked_signal_key() -> None:
    signal = Signal(
        workspace_id=1,
        key="do_primary",
        name="DO Primary",
        io_direction=SignalIODirection.DO,
        signal_metadata={},
    )
    entry = TestRunAllocationEntry(channel_id=12, signal=signal)

    bindings = SignalBindingService.build_bindings([entry])

    assert bindings == {"do_primary": 12}


def test_build_bindings_falls_back_to_metadata() -> None:
    entry = TestRunAllocationEntry(
        channel_id=7,
        signal_metadata={
            "snapshot_signal_key": "di_feedback",
        },
    )

    bindings = SignalBindingService.build_bindings([entry])

    assert bindings == {"di_feedback": 7}
