from __future__ import annotations

import asyncio
import time
from types import SimpleNamespace

from app.schemas.ws.messages import SetAoCommandMessage, WSAction
from app.ws.actions import manual_command_admission


def run_async(awaitable):
    return asyncio.run(awaitable)


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

    async def sender(command_id: str):
        calls.append("publish")

    async def result(*args, **kwargs):
        return None

    monkeypatch.setattr(manual_command_admission, "AsyncSessionLocal", lambda: Session())
    monkeypatch.setattr(manual_command_admission, "RedisManager", SimpleNamespace(get_instance=lambda: object()))
    monkeypatch.setattr(manual_command_admission, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(manual_command_admission, "record_hardware_command_intent", record_intent)
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_queued", mark_queued)
    monkeypatch.setattr(manual_command_admission, "_result", result)

    run_async(
        manual_command_admission._enqueue_manual(
            object(),
            workspace_id=7,
            channel_ids=[17],
            unit_id="unit-1",
            device_id=23,
            action="ao_set",
            payload={"value": 12.5},
            sender=sender,
        )
    )

    assert calls.index("publish") < calls.index("queued") < calls.index("release")


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
