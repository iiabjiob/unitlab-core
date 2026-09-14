from __future__ import annotations

from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest

from app.models.hardware_command import HardwareCommandIntent
from app.services.hardware_command_ack import record_hardware_command_ack, wait_for_hardware_command_acks
from app.services.hardware_command_intent import (
    has_hardware_recovery_required,
    mark_hardware_command_intent_delivery_failure,
    reconcile_unfinished_hardware_command_intents,
)
from app.workers.signal_test_run_runner import _deliver_durable_command, _wait_for_bit_readback


@pytest.mark.anyio
async def test_ack_updates_only_matching_command_and_unit() -> None:
    intent = HardwareCommandIntent(
        command_id="cmd-1",
        workspace_id=7,
        owner_kind="fat",
        owner_id="job-1",
        channel_id=101,
        unit_id="UNIT-1",
        action="do_set",
        payload={},
    )
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(scalar_one_or_none=lambda: intent)

    matched = await record_hardware_command_ack(
        db,
        command_id="cmd-1",
        unit_id="UNIT-1",
        packet_id=42,
        status="OK",
        error="NONE",
    )

    assert matched is True
    assert intent.execution_status == "acknowledged"
    assert intent.ack_packet_id == 42
    assert intent.ack_status == "OK"
    db.commit.assert_awaited_once()


@pytest.mark.anyio
async def test_ack_for_unknown_command_does_not_commit() -> None:
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(scalar_one_or_none=lambda: None)

    matched = await record_hardware_command_ack(
        db,
        command_id="missing",
        unit_id="UNIT-1",
        packet_id=42,
        status="BUSY",
        error="HW_FAILURE",
    )

    assert matched is False
    db.commit.assert_not_awaited()


@pytest.mark.anyio
async def test_wait_for_command_acks_returns_terminal_states() -> None:
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(
        all=lambda: [("cmd-1", "acknowledged"), ("cmd-2", "negative_ack")]
    )

    states = await wait_for_hardware_command_acks(
        db,
        command_ids=["cmd-1", "cmd-2"],
        timeout_ms=100,
    )

    assert states == {"cmd-1": "acknowledged", "cmd-2": "negative_ack"}


@pytest.mark.anyio
async def test_late_ack_does_not_overwrite_timeout() -> None:
    intent = HardwareCommandIntent(
        command_id="cmd-timeout",
        workspace_id=7,
        owner_kind="fat",
        owner_id="job-1",
        channel_id=101,
        unit_id="UNIT-1",
        action="do_set",
        payload={},
        execution_status="timeout",
    )
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(scalar_one_or_none=lambda: intent)

    matched = await record_hardware_command_ack(
        db,
        command_id="cmd-timeout",
        unit_id="UNIT-1",
        packet_id=43,
        status="OK",
        error="NONE",
    )

    assert matched is False
    assert intent.execution_status == "timeout"
    db.commit.assert_not_awaited()


@pytest.mark.anyio
async def test_delivery_failure_status_is_persisted() -> None:
    db = AsyncMock()

    await mark_hardware_command_intent_delivery_failure(
        db,
        command_id="cmd-restore",
        status="recovery_required",
    )

    db.execute.assert_awaited_once()
    db.flush.assert_awaited_once()


@pytest.mark.anyio
async def test_recovery_required_lookup_returns_channel_block() -> None:
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(scalar=lambda: True)

    blocked = await has_hardware_recovery_required(db, workspace_id=7, channel_id=101)

    assert blocked is True
    db.execute.assert_awaited_once()


@pytest.mark.anyio
async def test_restore_delivery_retries_once_with_same_command_id() -> None:
    db = AsyncMock()
    attempts: list[str] = []

    async def sender(command_id: str) -> None:
        attempts.append(command_id)
        if len(attempts) == 1:
            raise RuntimeError("stream unavailable")

    await _deliver_durable_command(
        db,
        command_id="cmd-restore",
        action="restore",
        command_sender=sender,
    )

    assert attempts == ["cmd-restore", "cmd-restore"]
    assert db.rollback.await_count == 1
    assert db.commit.await_count == 1


@pytest.mark.anyio
async def test_regular_delivery_does_not_retry_after_publish_failure() -> None:
    db = AsyncMock()
    attempts: list[str] = []

    async def sender(command_id: str) -> None:
        attempts.append(command_id)
        raise RuntimeError("stream unavailable")

    with pytest.raises(RuntimeError, match="stream unavailable"):
        await _deliver_durable_command(
            db,
            command_id="cmd-do",
            action="do_set",
            command_sender=sender,
        )

    assert attempts == ["cmd-do"]
    assert db.rollback.await_count == 1
    assert db.commit.await_count == 1


@pytest.mark.anyio
async def test_reconcile_unfinished_intents_marks_restore_for_recovery() -> None:
    restore = HardwareCommandIntent(
        command_id="cmd-restore",
        workspace_id=7,
        job_id="job-1",
        attempt_id="attempt-1",
        owner_kind="fat",
        owner_id="job-1",
        channel_id=101,
        unit_id="UNIT-1",
        action="restore",
        payload={},
        status="queued",
    )
    regular = HardwareCommandIntent(
        command_id="cmd-do",
        workspace_id=7,
        job_id="job-1",
        attempt_id="attempt-1",
        owner_kind="fat",
        owner_id="job-1",
        channel_id=102,
        unit_id="UNIT-1",
        action="do_set",
        payload={},
        status="created",
    )
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(scalars=lambda: SimpleNamespace(all=lambda: [restore, regular]))

    count = await reconcile_unfinished_hardware_command_intents(
        db,
        job_id="job-1",
        attempt_id="attempt-1",
    )

    assert count == 2
    assert restore.status == "recovery_required"
    assert regular.status == "unknown"
    db.flush.assert_awaited_once()


@pytest.mark.anyio
async def test_bit_readback_wait_accepts_expected_channel_value() -> None:
    class Redis:
        async def get(self, key: str):
            assert key == "device:UNIT-1:bitmask"
            return "4"

    assert await _wait_for_bit_readback(
        Redis(),
        unit_id="UNIT-1",
        channel_index=2,
        expected_value=1,
        timeout_ms=100,
    ) is True
