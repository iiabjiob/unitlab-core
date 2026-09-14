from __future__ import annotations

from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest

from app.models.hardware_command import HardwareCommandIntent, HardwareCommandIntentChannel
from app.infrastructure.redis.manager import RedisManager
from app.services.hardware_command_ack import record_hardware_command_ack, wait_for_hardware_command_acks
from app.services.hardware_command_ack import record_hardware_command_ack_diagnostic
from app.services.hardware_command_intent import (
    has_hardware_recovery_required,
    mark_hardware_command_intent_completed,
    mark_hardware_command_intent_delivery_failure,
    record_hardware_command_intent,
    reconcile_orphaned_manual_hardware_command_intents,
    reconcile_unfinished_hardware_command_intents,
    requires_physical_recovery,
)
from app.workers.signal_test_run_runner import (
    _deliver_durable_command,
    _wait_for_bit_readback,
    _wait_for_float_readback,
    _wait_for_fresh_bitmask_snapshot,
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
async def test_ack_diagnostic_is_appended_for_late_or_duplicate_response(monkeypatch) -> None:
    class FakeRedis:
        def __init__(self) -> None:
            self.calls = []

        async def xadd(self, stream, fields, **kwargs):
            self.calls.append((stream, fields, kwargs))
            return "1-0"

    redis = FakeRedis()
    monkeypatch.setattr(RedisManager, "get_instance", classmethod(lambda cls: redis))

    await record_hardware_command_ack_diagnostic(
        command_id="cmd-1",
        unit_id="UNIT-1",
        packet_id=12,
        status="OK",
        error="NONE",
        reason="late_duplicate_or_terminal_intent",
    )

    assert redis.calls[0][0] == "hardware:command-ack-diagnostics"
    assert redis.calls[0][1]["command_id"] == "cmd-1"
    assert redis.calls[0][1]["reason"] == "late_duplicate_or_terminal_intent"


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
async def test_wait_timeout_marks_command_for_recovery() -> None:
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(all=lambda: [])

    states = await wait_for_hardware_command_acks(
        db,
        command_ids=["cmd-do"],
        timeout_ms=100,
    )

    assert states == {"cmd-do": "timeout"}
    timeout_update = db.execute.await_args.args[0]
    assert "execution_status" in str(timeout_update)
    assert "status" in str(timeout_update)


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
async def test_completed_intent_status_is_persisted_as_terminal() -> None:
    db = AsyncMock()

    await mark_hardware_command_intent_completed(db, command_id="cmd-completed")

    statement = db.execute.await_args.args[0]
    compiled = str(statement.compile(compile_kwargs={"literal_binds": True}))
    assert "completed" in compiled
    assert "queued" in compiled
    assert "acknowledged" in compiled
    db.flush.assert_awaited_once()


@pytest.mark.anyio
async def test_interrupted_pulse_requires_physical_recovery() -> None:
    intent = SimpleNamespace(action="do_pulse", status="queued")
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(scalars=lambda: SimpleNamespace(all=lambda: [intent]))

    reconciled = await reconcile_unfinished_hardware_command_intents(db, job_id="run-1")

    assert reconciled == 1
    assert intent.status == "recovery_required"


@pytest.mark.anyio
async def test_orphaned_manual_intents_fail_closed_at_startup() -> None:
    manual = SimpleNamespace(owner_kind="manual", action="do_set", status="queued")
    pulse = SimpleNamespace(owner_kind="manual", action="do_pulse", status="created")
    completed = SimpleNamespace(owner_kind="manual", action="do_set", status="completed")
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(
        scalars=lambda: SimpleNamespace(all=lambda: [manual, pulse])
    )

    reconciled = await reconcile_orphaned_manual_hardware_command_intents(db)

    assert reconciled == 2
    assert manual.status == "unknown"
    assert pulse.status == "recovery_required"
    assert completed.status == "completed"
    statement = db.execute.await_args.args[0]
    compiled = str(statement.compile(compile_kwargs={"literal_binds": True}))
    assert "created" in compiled
    assert "queued" in compiled
    db.flush.assert_awaited_once()


def test_pulse_and_restore_require_physical_recovery() -> None:
    assert requires_physical_recovery("do_pulse") is True
    assert requires_physical_recovery("restore") is True
    assert requires_physical_recovery("do_set") is False


@pytest.mark.anyio
async def test_recovery_required_lookup_returns_channel_block() -> None:
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(scalar=lambda: True)

    blocked = await has_hardware_recovery_required(db, workspace_id=7, channel_id=101)

    assert blocked is True
    db.execute.assert_awaited_once()


@pytest.mark.anyio
async def test_pending_queued_command_is_part_of_recovery_lookup() -> None:
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(scalar=lambda: True)

    blocked = await has_hardware_recovery_required(db, workspace_id=7, channel_id=101)

    assert blocked is True
    assert "execution_status" in str(db.execute.await_args.args[0])


@pytest.mark.anyio
async def test_legacy_publish_failed_command_is_part_of_recovery_lookup() -> None:
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(scalar=lambda: True)

    blocked = await has_hardware_recovery_required(db, workspace_id=7, channel_id=101)

    assert blocked is True
    statement = db.execute.await_args.args[0]
    assert "publish_failed" in str(statement.compile(compile_kwargs={"literal_binds": True}))


@pytest.mark.anyio
async def test_multi_channel_recovery_lookup_blocks_secondary_channel() -> None:
    db = AsyncMock()
    db.execute.return_value = SimpleNamespace(scalar=lambda: True)

    blocked = await has_hardware_recovery_required(db, workspace_id=9, channel_id=202)

    assert blocked is True
    assert "hardware_command_intent_channels" in str(db.execute.await_args.args[0])


@pytest.mark.anyio
async def test_record_intent_persists_every_multi_channel_scope() -> None:
    db = AsyncMock()
    db.add = lambda item: db._added.append(item)
    db._added = []

    await record_hardware_command_intent(
        db,
        command_id="cmd-multi",
        workspace_id=7,
        job_id="run-1",
        attempt_id=None,
        owner_kind="sequence",
        owner_id="run-1",
        device_id=3,
        channel_id=101,
        unit_id="UNIT-1",
        action="do_pair",
        payload={"channel_ids": [101, 202]},
        fencing_epoch=1,
    )

    links = [item for item in db._added if isinstance(item, HardwareCommandIntentChannel)]
    assert [(item.command_id, item.channel_id) for item in links] == [
        ("cmd-multi", 101),
        ("cmd-multi", 202),
    ]


@pytest.mark.anyio
async def test_record_intent_rejects_malformed_multi_channel_scope() -> None:
    db = AsyncMock()

    with pytest.raises(ValueError, match="invalid channel"):
        await record_hardware_command_intent(
            db,
            command_id="cmd-invalid",
            workspace_id=7,
            job_id="run-1",
            attempt_id=None,
            owner_kind="sequence",
            owner_id="run-1",
            device_id=3,
            channel_id=101,
            unit_id="UNIT-1",
            action="do_pair",
            payload={"channel_ids": [101, "not-an-id"]},
            fencing_epoch=1,
        )

    db.flush.assert_not_awaited()


@pytest.mark.anyio
async def test_record_intent_rejects_empty_multi_channel_scope() -> None:
    db = AsyncMock()

    with pytest.raises(ValueError, match="must not be empty"):
        await record_hardware_command_intent(
            db,
            command_id="cmd-empty",
            workspace_id=7,
            job_id="run-1",
            attempt_id=None,
            owner_kind="sequence",
            owner_id="run-1",
            device_id=3,
            channel_id=101,
            unit_id="UNIT-1",
            action="do_all",
            payload={"channel_ids": []},
            fencing_epoch=1,
        )

    db.flush.assert_not_awaited()


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
async def test_pulse_publish_failure_is_recorded_as_recovery_required() -> None:
    db = AsyncMock()

    async def sender(command_id: str) -> None:
        del command_id
        raise RuntimeError("stream unavailable")

    with pytest.raises(RuntimeError, match="stream unavailable"):
        await _deliver_durable_command(
            db,
            command_id="cmd-pulse",
            action="do_pulse",
            command_sender=sender,
        )

    statement = db.execute.await_args.args[0]
    assert "recovery_required" in str(statement.compile(compile_kwargs={"literal_binds": True}))


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


@pytest.mark.anyio
async def test_bit_readback_rejects_old_packet_value() -> None:
    class Redis:
        async def get(self, key: str):
            if key.endswith(":last_state_packet_id"):
                return "40"
            return "4"

    assert await _wait_for_bit_readback(
        Redis(),
        unit_id="UNIT-1",
        channel_index=2,
        expected_value=1,
        timeout_ms=100,
        packet_id=41,
    ) is False


@pytest.mark.anyio
async def test_initial_bitmask_snapshot_rejects_old_packet_value() -> None:
    class Redis:
        async def get(self, key: str):
            if key.endswith(":last_state_packet_id"):
                return "40"
            return "4"

    assert await _wait_for_fresh_bitmask_snapshot(
        Redis(),
        unit_id="UNIT-1",
        packet_id=41,
        timeout_ms=100,
    ) is None


@pytest.mark.anyio
async def test_float_readback_wait_accepts_small_measurement_tolerance() -> None:
    class Redis:
        async def hget(self, key: str, field: str):
            assert key == "device:UNIT-1:ao"
            assert field == "3"
            return "12.345"

    assert await _wait_for_float_readback(
        Redis(),
        unit_id="UNIT-1",
        channel_index=3,
        expected_value=12.34,
        timeout_ms=100,
    ) is True
