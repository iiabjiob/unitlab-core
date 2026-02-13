from __future__ import annotations

from typing import Any

from app.models.test_run import TestRunAllocationEntry


class SignalBindingService:
    """Build `signal_key -> channel_id` bindings from allocation rows."""

    @staticmethod
    def build_bindings(entries: list[TestRunAllocationEntry]) -> dict[str, int]:
        bindings: dict[str, int] = {}
        for entry in entries:
            key = SignalBindingService.extract_signal_key(entry)
            if not key:
                continue
            bindings[key] = entry.channel_id
        return bindings

    @staticmethod
    def extract_signal_key(entry: TestRunAllocationEntry) -> str | None:
        if entry.signal is not None and entry.signal.key:
            return entry.signal.key

        metadata = entry.signal_metadata if isinstance(entry.signal_metadata, dict) else {}
        return SignalBindingService._extract_key_from_metadata(metadata)

    @staticmethod
    def _extract_key_from_metadata(metadata: dict[str, Any]) -> str | None:
        candidates = (
            metadata.get("snapshot_signal_key"),
            metadata.get("signal_key"),
            metadata.get("key"),
        )
        for value in candidates:
            if isinstance(value, str) and value.strip():
                return value.strip()
        return None
