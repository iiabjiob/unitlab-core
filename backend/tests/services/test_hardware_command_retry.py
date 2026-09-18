from datetime import datetime, timedelta, timezone
from types import SimpleNamespace
from unittest.mock import AsyncMock, Mock

import pytest
from sqlalchemy import create_engine, select
from sqlalchemy.orm import Session

from app.core.mqtt_dto import OutboundCmdMsg
from app.infrastructure.mqtt import outbound_worker
from app.models.hardware_command import HardwareCommandIntent, HardwareCommandIntentChannel
from app.services import hardware_command_intent as service
from app.workers import mqtt_outbound
from app.api.v1.signal_sheet import repository
from app.models.signal_sheet import SignalAllocation


NOW = datetime(2026, 9, 16, 12, tzinfo=timezone.utc)


@pytest.fixture
def db(monkeypatch):
    engine = create_engine("sqlite://")
    HardwareCommandIntent.__table__.create(engine)
    HardwareCommandIntentChannel.__table__.create(engine)
    monkeypatch.setattr(service, "hardware_retry_cutoff", lambda: NOW - timedelta(seconds=60))
    with Session(engine) as session:
        class Adapter:
            async def execute(self, statement):
                return session.execute(statement)

            async def __aenter__(self):
                return self

            async def __aexit__(self, *args):
                pass

        yield session, Adapter()
    engine.dispose()


def add_intent(session, *, age=61, command_id="old", **overrides):
    fields = dict(
        command_id=command_id, workspace_id=1, channel_id=5, unit_id="C45B64",
        owner_kind="manual", owner_id="manual:test", action="do_set",
        payload={"ch": 4, "value": 1}, status="unknown", execution_status="timeout",
        created_at=NOW - timedelta(seconds=age),
    )
    fields.update(overrides)
    intent = HardwareCommandIntent(**fields)
    session.add(intent)
    session.flush()
    session.add(HardwareCommandIntentChannel(command_id=command_id, channel_id=5))
    session.commit()
    return intent


@pytest.mark.anyio
@pytest.mark.parametrize("age,blocked", [(59, False), (60, False), (61, False), (86400, False)])
async def test_manual_latch_retry_boundary_preserves_timeout_evidence(db, age, blocked):
    session, adapter = db
    old = add_intent(session, age=age)
    result = await service.list_hardware_recovery_required_channels(
        adapter, channel_ids=[5, 6], action="do_set",
    )
    assert result == ({5} if blocked else set())
    session.refresh(old)
    assert (old.status, old.execution_status, old.payload) == ("unknown", "timeout", {"ch": 4, "value": 1})


@pytest.mark.anyio
@pytest.mark.parametrize("owner,old_action,status", [
    ("manual", "do_set", "unknown"),
    ("manual", "do_pulse", "recovery_required"),
    ("fat", "restore", "recovery_required"),
    ("fat", "do_set", "publish_failed"),
    ("sequence", "do_pair", "queued"),
    ("sequence", "ao_set", "created"),
])
@pytest.mark.parametrize("new_action", [None, "do_set", "do_pair", "do_all", "do_pulse", "ao_set"])
@pytest.mark.parametrize("age,blocked", [(59, True), (60, False)])
@pytest.mark.parametrize("execution", ["unknown", "timeout"])
async def test_all_control_paths_share_missing_ack_cooldown(
    db, owner, old_action, status, new_action, age, blocked, execution,
):
    session, adapter = db
    add_intent(session, owner_kind=owner, action=old_action, status=status, age=age, execution_status=execution)
    expected_blocked = blocked and execution != "timeout"
    assert await service.list_hardware_recovery_required_channels(
        adapter, channel_ids=[5], action=new_action,
    ) == ({5} if expected_blocked else set())
    assert await service.has_hardware_recovery_required(
        adapter, workspace_id=99, channel_id=5,
    ) is expected_blocked


@pytest.mark.anyio
@pytest.mark.parametrize("execution", ["acknowledged", "negative_ack"])
async def test_confirmed_failure_is_not_treated_as_missing_ack(db, execution):
    session, adapter = db
    add_intent(session, status="recovery_required", execution_status=execution)
    assert await service.list_hardware_recovery_required_channels(
        adapter, channel_ids=[5], action="do_set",
    ) == {5}


@pytest.mark.anyio
async def test_recent_timeout_does_not_block_after_an_older_timeout_expires(db):
    session, adapter = db
    add_intent(session)
    add_intent(session, command_id="new", age=5)
    assert await service.list_hardware_recovery_required_channels(
        adapter, channel_ids=[5], action="do_set",
    ) == set()


@pytest.mark.anyio
@pytest.mark.parametrize("age,status,execution,allowed", [
    (1, "created", "unknown", True), (59, "queued", "unknown", True),
    (60, "queued", "unknown", False), (61, "created", "unknown", False),
    (1, "unknown", "timeout", False), (1, "queued", "timeout", False),
    (1, "completed", "acknowledged", False), (1, "unknown", "unknown", False),
])
@pytest.mark.parametrize("owner,action", [("manual", "do_set"), ("manual", "do_pulse"), ("fat", "restore"), ("sequence", "do_pair")])
async def test_outbound_never_publishes_expired_or_terminal_command(
    db, monkeypatch, age, status, execution, allowed, owner, action,
):
    session, adapter = db
    add_intent(session, age=age, status=status, execution_status=execution, owner_kind=owner, action=action)
    monkeypatch.setattr(outbound_worker, "AsyncSessionLocal", lambda: adapter)
    mqtt = SimpleNamespace(publish=Mock())
    message = OutboundCmdMsg(topic="C45B64/c", payload=b"frame", command_id="old")
    await outbound_worker.publish_outbound_message(mqtt, message)
    assert mqtt.publish.call_count == int(allowed)


@pytest.mark.anyio
async def test_state_requests_do_not_need_a_database_lookup(monkeypatch):
    monkeypatch.setattr(outbound_worker, "AsyncSessionLocal", Mock(side_effect=AssertionError))
    mqtt = SimpleNamespace(publish=Mock())
    await outbound_worker.publish_outbound_message(mqtt, OutboundCmdMsg(topic="C45B64/q", payload=b"state"))
    mqtt.publish.assert_called_once()


@pytest.mark.anyio
async def test_expired_stream_entry_is_acknowledged_without_replaying_it(db, monkeypatch):
    session, adapter = db
    add_intent(session)
    monkeypatch.setattr(outbound_worker, "AsyncSessionLocal", lambda: adapter)
    message = OutboundCmdMsg(topic="C45B64/c", payload=b"frame", command_id="old")
    monkeypatch.setattr(mqtt_outbound, "parse_outbound_entry", lambda entry: ("1-0", message))
    redis = SimpleNamespace(xack=AsyncMock())
    mqtt = SimpleNamespace(publish=Mock())
    await mqtt_outbound._process_entries(redis, mqtt, [("1-0", {})])
    mqtt.publish.assert_not_called()
    redis.xack.assert_awaited_once_with(mqtt_outbound.STREAM_NAME, mqtt_outbound.GROUP_NAME, "1-0")


@pytest.mark.anyio
async def test_database_failure_does_not_publish_or_discard_the_command(monkeypatch):
    monkeypatch.setattr(outbound_worker, "AsyncSessionLocal", Mock(side_effect=RuntimeError("DB unavailable")))
    message = OutboundCmdMsg(topic="C45B64/c", payload=b"frame", command_id="old")
    monkeypatch.setattr(mqtt_outbound, "parse_outbound_entry", lambda entry: ("1-0", message))
    redis = SimpleNamespace(xack=AsyncMock())
    mqtt = SimpleNamespace(publish=Mock())
    await mqtt_outbound._process_entries(redis, mqtt, [("1-0", {})])
    mqtt.publish.assert_not_called()
    redis.xack.assert_not_awaited()


@pytest.mark.anyio
@pytest.mark.parametrize("age,blocked", [(59, False), (60, False), (86400, False)])
async def test_signal_test_binding_applies_cooldown_in_its_correlated_sql(db, monkeypatch, age, blocked):
    session, _ = db
    add_intent(session, age=age, action="do_pulse", owner_kind="fat", status="recovery_required")
    SignalAllocation.__table__.create(session.bind)
    session.execute(SignalAllocation.__table__.insert(), [
        {"id": 1, "workspace_id": 1, "signal_id": 7, "channel_id": 5},
        {"id": 2, "workspace_id": 1, "signal_id": 8, "channel_id": 6},
    ])
    allocation = SimpleNamespace(
        id=1, channel_id=5, device_id=2,
        channel=SimpleNamespace(id=5, device_id=2, channel_index=4,
                                device=SimpleNamespace(id=2, unit_id="C45B64")),
    )

    class BindingDb:
        async def execute(self, statement):
            # Evaluate the real repository's correlated recovery expression in SQL;
            # mock only the unrelated allocation hydration and device presence.
            rows = session.execute(select(
                SignalAllocation.id, statement.selected_columns.recovery_required,
            ).order_by(SignalAllocation.id)).all()
            assert rows == [(1, blocked), (2, False)]
            return SimpleNamespace(first=lambda: (allocation, rows[0][1]))

    monkeypatch.setattr(repository, "DevicePresenceService", lambda: SimpleNamespace(
        get_presence=AsyncMock(return_value=SimpleNamespace(online=True)),
    ))
    binding = await repository.SignalSheetRepository(BindingDb()).get_execution_binding_with_recovery(1, 7)
    assert binding.recovery_required is blocked
