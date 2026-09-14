from __future__ import annotations

import asyncio
import time
from types import SimpleNamespace

import pytest

from app.schemas.ws.messages import SetAoCommandMessage, WSAction
from app.ws.actions import manual_command_admission


def run_async(awaitable):
    return asyncio.run(awaitable)


@pytest.fixture(autouse=True)
def manual_recovery_barrier_default(monkeypatch):
    async def no_blocked_channels(*args, **kwargs):
        return set()

    monkeypatch.setattr(
        manual_command_admission,
        "list_hardware_recovery_required_channels",
        no_blocked_channels,
    )


def test_manual_ao_passes_single_channel_as_channel_ids(monkeypatch) -> None:
    captured: dict = {}

    async def channel(*args, **kwargs):
        return SimpleNamespace(id=17, device_id=23)

    async def enqueue_manual(ws, **kwargs):
        captured.update(kwargs)

    async def online_status(key: str):
        return "online" if key.endswith(":status") else str(int(time.time() * 1000))

    redis = SimpleNamespace(get=online_status)

    monkeypatch.setattr(manual_command_admission, "_channel", channel)
    monkeypatch.setattr(manual_command_admission, "_enqueue_manual", enqueue_manual)
    monkeypatch.setattr(
        manual_command_admission,
        "RedisManager",
        SimpleNamespace(get_instance=lambda: redis),
    )

    message = SetAoCommandMessage(
        action=WSAction.SET_AO_COMMAND,
        workspace_id=7,
        channel_id=17,
        unit_id="unit-1",
        ch=2,
        value=12.5,
    )
    run_async(manual_command_admission.handle_manual_ao(object(), message))

    assert captured["channel_ids"] == [17]
    assert captured["device_id"] == 23
    assert captured["action"] == "ao_set"


def test_manual_device_status_must_be_online(monkeypatch) -> None:
    async def status(key: str):
        return "offline" if key.endswith(":status") else str(int(time.time() * 1000))

    monkeypatch.setattr(
        manual_command_admission,
        "RedisManager",
        SimpleNamespace(get_instance=lambda: SimpleNamespace(get=status)),
    )

    assert run_async(manual_command_admission._device_is_online("unit-1")) is False


def test_manual_device_with_stale_last_seen_is_not_online(monkeypatch) -> None:
    async def status(key: str):
        return "online" if key.endswith(":status") else "1"

    monkeypatch.setattr(
        manual_command_admission,
        "RedisManager",
        SimpleNamespace(get_instance=lambda: SimpleNamespace(get=status)),
    )

    assert run_async(manual_command_admission._device_is_online("unit-1")) is False


def test_manual_readback_rejects_matching_value_from_old_packet() -> None:
    class Redis:
        async def get(self, key: str):
            if key.endswith(":last_state_packet_id"):
                return "41"
            return "0"

    assert run_async(
        manual_command_admission._wait_for_manual_readback(
            Redis(),
            action="do_set",
            unit_id="unit-1",
            channel_index=0,
            expected_value=0,
            timeout_ms=100,
            packet_id=42,
        )
    ) is False


def test_manual_command_marks_intent_queued_after_publish(monkeypatch) -> None:
    calls: list[str] = []

    class Session:
        async def __aenter__(self):
            return self

        async def __aexit__(self, exc_type, exc, tb):
            return False

        async def commit(self):
            calls.append("commit")

    class Admission:
        def __init__(self, redis) -> None:
            return None

        async def acquire_many(self, **kwargs):
            return [SimpleNamespace(fencing_epoch=4, lease_id="lease-1")]

        async def release(self, lease):
            calls.append("release")
            return True

    async def record_intent(*args, **kwargs):
        calls.append("record")

    async def mark_queued(*args, **kwargs):
        calls.append("queued")

    async def mark_completed(*args, **kwargs):
        calls.append("completed")

    async def sender(command_id: str):
        calls.append("publish")

    async def result(*args, **kwargs):
        return None

    monkeypatch.setattr(manual_command_admission, "AsyncSessionLocal", lambda: Session())
    monkeypatch.setattr(manual_command_admission, "RedisManager", SimpleNamespace(get_instance=lambda: object()))
    monkeypatch.setattr(manual_command_admission, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(manual_command_admission, "record_hardware_command_intent", record_intent)
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_queued", mark_queued)
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_completed", mark_completed)
    async def wait_for_acks(*args, **kwargs):
        calls.append("acknowledged")
        return {kwargs["command_ids"][0]: "acknowledged"}

    monkeypatch.setattr(manual_command_admission, "wait_for_hardware_command_acks", wait_for_acks)
    monkeypatch.setattr(manual_command_admission, "enqueue_request_state", _done)
    monkeypatch.setattr(manual_command_admission, "_wait_for_manual_readback", lambda *args, **kwargs: _done_true())
    monkeypatch.setattr(manual_command_admission, "_result", result)

    run_async(
        manual_command_admission._enqueue_manual(
            object(),
            workspace_id=7,
            channel_ids=[17],
            unit_id="unit-1",
            device_id=23,
            action="ao_set",
            payload={"ch": 2, "value": 12.5},
            sender=sender,
        )
    )

    assert calls.index("publish") < calls.index("queued") < calls.index("acknowledged") < calls.index("completed") < calls.index("release")


def test_manual_command_timeout_is_reported_and_lease_released(monkeypatch) -> None:
    captured: dict = {}

    class Session:
        async def __aenter__(self):
            return self

        async def __aexit__(self, exc_type, exc, tb):
            return False

        async def commit(self):
            return None

    class Admission:
        def __init__(self, redis) -> None:
            return None

        async def acquire_many(self, **kwargs):
            return [SimpleNamespace(fencing_epoch=1, lease_id="lease-1")]

        async def release(self, lease):
            return True

    async def wait_for_acks(*args, **kwargs):
        return {kwargs["command_ids"][0]: "timeout"}

    async def result(*args, **kwargs):
        captured.update(kwargs)

    monkeypatch.setattr(manual_command_admission, "AsyncSessionLocal", lambda: Session())
    monkeypatch.setattr(manual_command_admission, "RedisManager", SimpleNamespace(get_instance=lambda: object()))
    monkeypatch.setattr(manual_command_admission, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(manual_command_admission, "record_hardware_command_intent", lambda *a, **k: _done())
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_queued", lambda *a, **k: _done())
    monkeypatch.setattr(manual_command_admission, "wait_for_hardware_command_acks", wait_for_acks)
    monkeypatch.setattr(manual_command_admission, "_result", result)

    async def sender(command_id: str):
        return None

    run_async(
        manual_command_admission._enqueue_manual(
            object(),
            workspace_id=7,
            channel_ids=[17],
            unit_id="unit-1",
            device_id=23,
            action="do_set",
            payload={"value": 1},
            sender=sender,
        )
    )

    assert captured["delivery"] == "queued"
    assert captured["reason"] == "hardware_ack_timeout"


def test_manual_command_marks_recovery_when_acknowledged_readback_fails(monkeypatch) -> None:
    captured: dict = {}
    statuses: list[str] = []

    class Session:
        async def __aenter__(self):
            return self

        async def __aexit__(self, exc_type, exc, tb):
            return False

        async def commit(self):
            return None

    class Admission:
        def __init__(self, redis) -> None:
            return None

        async def acquire_many(self, **kwargs):
            return [SimpleNamespace(fencing_epoch=1, lease_id="lease-1")]

        async def release(self, lease):
            return True

    async def mark_failure(*args, **kwargs):
        statuses.append(kwargs["status"])

    async def result(*args, **kwargs):
        captured.update(kwargs)

    async def wait_for_acks(*args, **kwargs):
        return {kwargs["command_ids"][0]: "acknowledged"}

    async def readback(*args, **kwargs):
        return False

    async def noop(*args, **kwargs):
        return None

    monkeypatch.setattr(manual_command_admission, "AsyncSessionLocal", lambda: Session())
    monkeypatch.setattr(manual_command_admission, "RedisManager", SimpleNamespace(get_instance=lambda: object()))
    monkeypatch.setattr(manual_command_admission, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(manual_command_admission, "record_hardware_command_intent", noop)
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_queued", noop)
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_delivery_failure", mark_failure)
    monkeypatch.setattr(manual_command_admission, "wait_for_hardware_command_acks", wait_for_acks)
    monkeypatch.setattr(manual_command_admission, "enqueue_request_state", noop)
    monkeypatch.setattr(manual_command_admission, "_wait_for_manual_readback", readback)
    monkeypatch.setattr(manual_command_admission, "_result", result)

    run_async(
        manual_command_admission._enqueue_manual(
            object(),
            workspace_id=7,
            channel_ids=[17],
            unit_id="unit-1",
            device_id=23,
            action="do_set",
            payload={"ch": 2, "value": 1},
            sender=lambda command_id: _done(),
        )
    )

    assert statuses == ["recovery_required"]
    assert captured["reason"] == "hardware_readback_timeout"


def test_manual_command_rejects_channel_with_recovery_required_intent(monkeypatch) -> None:
    captured: dict = {}
    acquire_calls: list[dict] = []

    async def blocked_channels(*args, **kwargs):
        return {17}

    async def result(*args, **kwargs):
        captured.update(kwargs)

    class Admission:
        def __init__(self, redis) -> None:
            del redis

        async def acquire_many(self, **kwargs):
            acquire_calls.append(kwargs)
            raise AssertionError("recovery barrier must run before lease admission")

    monkeypatch.setattr(manual_command_admission, "list_hardware_recovery_required_channels", blocked_channels)
    monkeypatch.setattr(manual_command_admission, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(manual_command_admission, "RedisManager", SimpleNamespace(get_instance=lambda: object()))
    monkeypatch.setattr(manual_command_admission, "_result", result)

    run_async(
        manual_command_admission._enqueue_manual(
            object(),
            workspace_id=7,
            channel_ids=[17],
            unit_id="unit-1",
            device_id=23,
            action="do_set",
            payload={"ch": 2, "value": 1},
            sender=lambda command_id: _done(),
        )
    )

    assert acquire_calls == []
    assert captured == {
        "command_id": None,
        "delivery": "rejected",
        "reason": "hardware_recovery_required",
    }


async def _done(*args, **kwargs):
    return None


async def _done_true():
    return True


def test_manual_channels_reject_duplicate_scope_before_database_query(monkeypatch) -> None:
    async def unexpected_session():
        raise AssertionError("duplicate scope must be rejected before querying channels")

    monkeypatch.setattr(manual_command_admission, "AsyncSessionLocal", unexpected_session)

    assert run_async(
        manual_command_admission._channels(
            7,
            [17, 17],
            "unit-1",
            [0, 1],
            "do",
        )
    ) is None
