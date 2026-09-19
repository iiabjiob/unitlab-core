from datetime import datetime, timedelta, timezone
from collections.abc import Generator
from types import SimpleNamespace
from typing import Any, cast, final
from unittest.mock import AsyncMock, Mock

import pytest
from sqlalchemy import Table, create_engine, select
from sqlalchemy.orm import Session

from app.core.mqtt_dto import OutboundCmdMsg
from app.infrastructure.mqtt import outbound_worker
from app.infrastructure.mqtt.gmqtt_client import UnitLabMqttClient
from app.infrastructure.redis.types import RedisStreamClient
from app.models.hardware_command import HardwareCommandIntent, HardwareCommandIntentChannel
from app.services import hardware_command_intent as service
from app.workers import mqtt_outbound
from app.api.v1.signal_sheet import repository
from app.models.signal_sheet import SignalAllocation
from sqlalchemy.ext.asyncio import AsyncSession


NOW = datetime(2026, 9, 16, 12, tzinfo=timezone.utc)


@pytest.fixture
def db(monkeypatch: pytest.MonkeyPatch) -> Generator[tuple[Session, Any], None, None]:  # pyright: ignore[reportExplicitAny]
    engine = create_engine("sqlite://")
    cast(Table, HardwareCommandIntent.__table__).create(engine)
    cast(Table, HardwareCommandIntentChannel.__table__).create(engine)
    monkeypatch.setattr(service, "hardware_retry_cutoff", lambda: NOW - timedelta(seconds=60))
    with Session(engine) as session:
        @final
        class Adapter:
            async def execute(self, statement: Any) -> Any:  # pyright: ignore[reportAny,reportExplicitAny]
                return session.execute(statement)  # pyright: ignore[reportAny]

            async def __aenter__(self) -> "Adapter":
                return self

            async def __aexit__(self, *args: object) -> None:
                return None

        yield session, Adapter()
    engine.dispose()


def add_intent(session: Session, *, age: int = 61, command_id: str = "old", **overrides: object) -> HardwareCommandIntent:
    fields: dict[str, object] = dict(
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
async def test_manual_latch_retry_boundary_preserves_timeout_evidence(db: tuple[Session, Any], age: int, blocked: bool) -> None:  # pyright: ignore[reportExplicitAny]
    session, adapter = db  # pyright: ignore[reportAny]
    old = add_intent(session, age=age)
    result = await service.list_hardware_recovery_required_channels(
        adapter, channel_ids=[5, 6], action="do_set",  # pyright: ignore[reportAny]
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
    db: tuple[Session, Any], owner: str, old_action: str, status: str, new_action: str | None, age: int, blocked: bool, execution: str,  # pyright: ignore[reportExplicitAny]
):
    session, adapter = db  # pyright: ignore[reportAny]
    _ = add_intent(session, owner_kind=owner, action=old_action, status=status, age=age, execution_status=execution)
    expected_blocked = blocked and execution != "timeout"
    assert await service.list_hardware_recovery_required_channels(
        adapter, channel_ids=[5], action=new_action,  # pyright: ignore[reportAny]
    ) == ({5} if expected_blocked else set())
    assert await service.has_hardware_recovery_required(
        adapter, workspace_id=99, channel_id=5,  # pyright: ignore[reportAny]
    ) is expected_blocked


@pytest.mark.anyio
@pytest.mark.parametrize("execution", ["acknowledged", "negative_ack"])
async def test_confirmed_failure_is_not_treated_as_missing_ack(db: tuple[Session, Any], execution: str) -> None:  # pyright: ignore[reportExplicitAny]
    session, adapter = db  # pyright: ignore[reportAny]
    _ = add_intent(session, status="recovery_required", execution_status=execution)
    assert await service.list_hardware_recovery_required_channels(
        adapter, channel_ids=[5], action="do_set",  # pyright: ignore[reportAny]
    ) == {5}


@pytest.mark.anyio
async def test_recent_timeout_does_not_block_after_an_older_timeout_expires(db: tuple[Session, Any]) -> None:  # pyright: ignore[reportExplicitAny]
    session, adapter = db  # pyright: ignore[reportAny]
    _ = add_intent(session)
    _ = add_intent(session, command_id="new", age=5)
    assert await service.list_hardware_recovery_required_channels(
        adapter, channel_ids=[5], action="do_set",  # pyright: ignore[reportAny]
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
    db: tuple[Session, Any], monkeypatch: pytest.MonkeyPatch, age: int, status: str, execution: str, allowed: bool, owner: str, action: str,  # pyright: ignore[reportExplicitAny]
):
    session, adapter = db  # pyright: ignore[reportAny]
    _ = add_intent(session, age=age, status=status, execution_status=execution, owner_kind=owner, action=action)
    monkeypatch.setattr(outbound_worker, "AsyncSessionLocal", lambda: adapter)  # pyright: ignore[reportAny]
    mqtt = SimpleNamespace(publish=Mock())
    message = OutboundCmdMsg(topic="C45B64/c", payload=b"frame", command_id="old")
    await outbound_worker.publish_outbound_message(cast(UnitLabMqttClient, cast(object, mqtt)), message)
    assert mqtt.publish.call_count == int(allowed)  # pyright: ignore[reportAny]


@pytest.mark.anyio
async def test_state_requests_do_not_need_a_database_lookup(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(outbound_worker, "AsyncSessionLocal", Mock(side_effect=AssertionError))
    mqtt = SimpleNamespace(publish=Mock())
    await outbound_worker.publish_outbound_message(cast(UnitLabMqttClient, cast(object, mqtt)), OutboundCmdMsg(topic="C45B64/q", payload=b"state"))
    mqtt.publish.assert_called_once()  # pyright: ignore[reportAny]


@pytest.mark.anyio
async def test_expired_stream_entry_is_acknowledged_without_replaying_it(db: tuple[Session, Any], monkeypatch: pytest.MonkeyPatch) -> None:  # pyright: ignore[reportExplicitAny]
    session, adapter = db  # pyright: ignore[reportAny]
    _ = add_intent(session)
    monkeypatch.setattr(outbound_worker, "AsyncSessionLocal", lambda: adapter)  # pyright: ignore[reportAny]
    message = OutboundCmdMsg(topic="C45B64/c", payload=b"frame", command_id="old")
    def parse_entry(_entry: tuple[str, dict[str, object]]) -> tuple[str, OutboundCmdMsg]:
        return "1-0", message

    monkeypatch.setattr(mqtt_outbound, "parse_outbound_entry", parse_entry)
    redis = SimpleNamespace(xack=AsyncMock())
    mqtt = SimpleNamespace(publish=Mock())
    await mqtt_outbound._process_entries(  # pyright: ignore[reportPrivateUsage]
        cast(RedisStreamClient, cast(object, redis)),
        cast(UnitLabMqttClient, cast(object, mqtt)),
        [("1-0", {})],
    )
    mqtt.publish.assert_not_called()  # pyright: ignore[reportAny]
    redis.xack.assert_awaited_once_with(mqtt_outbound.STREAM_NAME, mqtt_outbound.GROUP_NAME, "1-0")  # pyright: ignore[reportAny]


@pytest.mark.anyio
async def test_database_failure_does_not_publish_or_discard_the_command(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(outbound_worker, "AsyncSessionLocal", Mock(side_effect=RuntimeError("DB unavailable")))
    message = OutboundCmdMsg(topic="C45B64/c", payload=b"frame", command_id="old")
    def parse_entry(_entry: tuple[str, dict[str, object]]) -> tuple[str, OutboundCmdMsg]:
        return "1-0", message

    monkeypatch.setattr(mqtt_outbound, "parse_outbound_entry", parse_entry)
    redis = SimpleNamespace(xack=AsyncMock())
    mqtt = SimpleNamespace(publish=Mock())
    await mqtt_outbound._process_entries(  # pyright: ignore[reportPrivateUsage]
        cast(RedisStreamClient, cast(object, redis)),
        cast(UnitLabMqttClient, cast(object, mqtt)),
        [("1-0", {})],
    )
    mqtt.publish.assert_not_called()  # pyright: ignore[reportAny]
    redis.xack.assert_not_awaited()  # pyright: ignore[reportAny]


@pytest.mark.anyio
@pytest.mark.parametrize("age,blocked", [(59, False), (60, False), (86400, False)])
async def test_signal_test_binding_applies_cooldown_in_its_correlated_sql(db: tuple[Session, Any], monkeypatch: pytest.MonkeyPatch, age: int, blocked: bool) -> None:  # pyright: ignore[reportExplicitAny]
    session, _ = db  # pyright: ignore[reportAny]
    _ = add_intent(session, age=age, action="do_pulse", owner_kind="fat", status="recovery_required")
    cast(Table, SignalAllocation.__table__).create(cast(Any, session.bind))  # pyright: ignore[reportAny,reportExplicitAny]
    signal_allocation_table = cast(Table, SignalAllocation.__table__)
    _ = session.execute(signal_allocation_table.insert(), [
        {"id": 1, "workspace_id": 1, "signal_id": 7, "channel_id": 5},
        {"id": 2, "workspace_id": 1, "signal_id": 8, "channel_id": 6},
    ])
    allocation = SimpleNamespace(
        id=1, channel_id=5, device_id=2,
        channel=SimpleNamespace(id=5, device_id=2, channel_index=4,
                                device=SimpleNamespace(id=2, unit_id="C45B64")),
    )

    class BindingDb:
        async def execute(self, statement):  # pyright: ignore[reportMissingParameterType,reportUnknownParameterType]
            # Evaluate the real repository's correlated recovery expression in SQL;
            # mock only the unrelated allocation hydration and device presence.
            rows = session.execute(select(  # pyright: ignore[reportUnknownVariableType,reportUnknownArgumentType]
                SignalAllocation.id, statement.selected_columns.recovery_required,  # pyright: ignore[reportUnknownArgumentType,reportUnknownMemberType]
            ).order_by(SignalAllocation.id)).all()
            assert rows == [(1, blocked), (2, False)]
            return SimpleNamespace(first=lambda: (allocation, rows[0][1]))

    monkeypatch.setattr(repository, "DevicePresenceService", lambda: SimpleNamespace(
        get_presence=AsyncMock(return_value=SimpleNamespace(online=True)),
    ))
    binding = await repository.SignalSheetRepository(cast(AsyncSession, cast(object, BindingDb()))).get_execution_binding_with_recovery(1, 7)
    assert binding is not None
    assert binding.recovery_required is blocked
