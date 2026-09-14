from __future__ import annotations

from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest

from app.models.hardware_command import HardwareCommandIntent
from app.services.hardware_command_ack import record_hardware_command_ack, wait_for_hardware_command_acks
from app.services.hardware_command_intent import (
    has_hardware_recovery_required,
    mark_hardware_command_intent_delivery_failure,
)


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
