from __future__ import annotations

import asyncio
import time
from types import SimpleNamespace
from collections.abc import Coroutine
from typing import cast, final

import pytest
from fastapi import WebSocket

from app.infrastructure.protocol.modes import State
from app.schemas.ws.messages import SetAoCommandMessage, WSAction
from app.ws.actions import manual_command_admission
from app.ws.actions.manual_command_admission import _ManualRedis  # pyright: ignore[reportPrivateUsage]


def run_async(awaitable: Coroutine[object, object, object]) -> object:
    return asyncio.run(awaitable)


async def _ttl(_key: str) -> int:
    return 30


async def _ttl_zero(_key: str) -> int:
    return 0


@pytest.fixture(autouse=True)
def manual_recovery_barrier_default(monkeypatch: pytest.MonkeyPatch) -> None:
    async def no_blocked_channels(*_args: object, **_kwargs: object) -> set[int]:
        return set()

    monkeypatch.setattr(
        manual_command_admission,
        "list_hardware_recovery_required_channels",
        no_blocked_channels,
    )


def test_manual_ao_passes_single_channel_as_channel_ids(monkeypatch: pytest.MonkeyPatch) -> None:
    captured: dict[str, object] = {}

    async def channel(*_args: object, **_kwargs: object) -> SimpleNamespace:
        return SimpleNamespace(id=17, device_id=23)

    async def enqueue_manual(_ws: object, **kwargs: object) -> None:
        captured.update(kwargs)

    async def online_status(key: str) -> str:
        return "online" if key.endswith(":status") else str(int(time.time() * 1000))

    redis = SimpleNamespace(get=online_status, ttl=_ttl)

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
    _ = run_async(manual_command_admission.handle_manual_ao(cast(WebSocket, object()), message))

    assert captured["channel_ids"] == [17]
    assert captured["device_id"] == 23
    assert captured["action"] == "ao_set"


def test_manual_device_status_must_be_online(monkeypatch: pytest.MonkeyPatch) -> None:
    async def status(key: str) -> str:
        return "offline" if key.endswith(":status") else str(int(time.time() * 1000))

    monkeypatch.setattr(
        manual_command_admission,
        "RedisManager",
        SimpleNamespace(get_instance=lambda: SimpleNamespace(get=status, ttl=_ttl)),
    )

    assert run_async(manual_command_admission._device_is_online("unit-1")) is False  # pyright: ignore[reportPrivateUsage]


def test_manual_device_with_stale_last_seen_is_not_online(monkeypatch: pytest.MonkeyPatch) -> None:
    async def status(key: str) -> str:
        return "online" if key.endswith(":status") else "1"

    monkeypatch.setattr(
        manual_command_admission,
        "RedisManager",
        SimpleNamespace(get_instance=lambda: SimpleNamespace(get=status, ttl=_ttl_zero)),
    )

    assert run_async(manual_command_admission._device_is_online("unit-1")) is False  # pyright: ignore[reportPrivateUsage]


def test_manual_readback_rejects_matching_value_from_old_packet() -> None:
    @final
    class Redis:
        async def get(self, key: str) -> str:
            if key.endswith(":last_state_packet_id"):
                return "41"
            return "0"

    assert run_async(
        manual_command_admission._wait_for_manual_readback(  # pyright: ignore[reportPrivateUsage]
            cast(_ManualRedis, cast(object, Redis())),
            action="do_set",
            unit_id="unit-1",
            channel_index=0,
            expected_value=0,
            timeout_ms=100,
            packet_id=42,
        )
    ) is False


def test_manual_bitmask_readback_accepts_one_fresh_bulk_snapshot() -> None:
    @final
    class Redis:
        async def get(self, key: str) -> str:
            if key.endswith(":last_state_packet_id"):
                return "42"
            return str(0x80000005)

    assert run_async(
        manual_command_admission._wait_for_manual_bitmask_readback(  # pyright: ignore[reportPrivateUsage]
            cast(_ManualRedis, cast(object, Redis())),
            unit_id="unit-1",
            expected_bitmask=0x80000005,
            channel_count=32,
            timeout_ms=100,
            packet_id=42,
        )
    ) is True


def test_manual_bitmask_readback_rejects_matching_mask_from_old_packet() -> None:
    @final
    class Redis:
        async def get(self, key: str) -> str:
            if key.endswith(":last_state_packet_id"):
                return "41"
            return "5"

    assert run_async(
        manual_command_admission._wait_for_manual_bitmask_readback(  # pyright: ignore[reportPrivateUsage]
            cast(_ManualRedis, cast(object, Redis())),
            unit_id="unit-1",
            expected_bitmask=5,
            channel_count=3,
            timeout_ms=100,
            packet_id=42,
        )
    ) is False


def test_manual_do_all_retries_only_bulk_readback(monkeypatch: pytest.MonkeyPatch) -> None:
    requests: list[dict[str, object]] = []
    readback_attempts: list[int] = []
    completed: list[str] = []

    @final
    class Session:
        async def __aenter__(self) -> "Session":
            return self

        async def __aexit__(self, exc_type: object, exc: BaseException | None, tb: object) -> bool:
            return False

        async def commit(self) -> None:
            return None

    @final
    class Admission:
        def __init__(self, redis: object) -> None:
            del redis

        async def acquire_many(self, **_kwargs: object) -> list[SimpleNamespace]:
            return [SimpleNamespace(fencing_epoch=1, lease_id="lease-1")]

        async def release(self, _lease: object) -> bool:
            return True

    async def wait_for_acks(*_args: object, **kwargs: object) -> dict[object, str]:
        return {cast(list[object], kwargs["command_ids"])[0]: "acknowledged"}

    async def enqueue_state(**kwargs: object) -> int:
        requests.append(kwargs)
        return 42

    async def bulk_readback(*_args: object, **kwargs: object) -> bool:
        assert kwargs["expected_bitmask"] == 5
        assert kwargs["channel_count"] == 3
        assert kwargs["packet_id"] == 42
        readback_attempts.append(cast(int, kwargs["packet_id"]))
        return len(readback_attempts) == 2

    async def unexpected_single_readback(*_args: object, **_kwargs: object) -> bool:
        raise AssertionError("DO ALL must not issue per-channel readbacks")

    async def mark_completed(*_args: object, **kwargs: object) -> None:
        completed.append(cast(str, kwargs["command_id"]))

    monkeypatch.setattr(manual_command_admission, "AsyncSessionLocal", lambda: Session())
    monkeypatch.setattr(manual_command_admission, "RedisManager", SimpleNamespace(get_instance=lambda: object()))
    monkeypatch.setattr(manual_command_admission, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(manual_command_admission, "record_hardware_command_intent", _done)
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_queued", _done)
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_completed", mark_completed)
    monkeypatch.setattr(manual_command_admission, "wait_for_hardware_command_acks", wait_for_acks)
    monkeypatch.setattr(manual_command_admission, "enqueue_request_state", enqueue_state)
    monkeypatch.setattr(manual_command_admission, "_wait_for_manual_bitmask_readback", bulk_readback)
    monkeypatch.setattr(manual_command_admission, "_wait_for_manual_readback", unexpected_single_readback)
    monkeypatch.setattr(manual_command_admission, "_result", _done)

    _ = run_async(
        manual_command_admission._enqueue_manual(  # pyright: ignore[reportPrivateUsage]
            cast(WebSocket, object()),
            workspace_id=7,
            channel_ids=[17, 18, 19],
            unit_id="unit-1",
            device_id=23,
            action="do_all",
            payload={"bitmask": 5, "channel_ids": [17, 18, 19]},
            sender=_done_sender,
        )
    )

    assert len(requests) == 2
    assert all(request["mode"] == State.REQ_ALL_BIT for request in requests)
    assert str(requests[0]["correlation_id"]).endswith(":readback:all:1")
    assert str(requests[1]["correlation_id"]).endswith(":readback:all:2")
    assert completed and len(completed) == 1


def test_manual_command_marks_intent_queued_after_publish(monkeypatch: pytest.MonkeyPatch) -> None:
    calls: list[str] = []

    @final
    class Session:
        async def __aenter__(self) -> "Session":
            return self

        async def __aexit__(self, exc_type: object, exc: BaseException | None, tb: object) -> bool:
            return False

        async def commit(self) -> None:
            calls.append("commit")

    @final
    class Admission:
        def __init__(self, redis: object) -> None:
            return None

        async def acquire_many(self, **_kwargs: object) -> list[SimpleNamespace]:
            return [SimpleNamespace(fencing_epoch=4, lease_id="lease-1")]

        async def release(self, _lease: object) -> bool:
            calls.append("release")
            return True

    async def record_intent(*_args: object, **_kwargs: object) -> None:
        calls.append("record")

    async def mark_queued(*_args: object, **_kwargs: object) -> None:
        calls.append("queued")

    async def mark_completed(*_args: object, **_kwargs: object) -> None:
        calls.append("completed")

    async def sender(command_id: str) -> str:
        calls.append("publish")
        return command_id

    async def result(*_args: object, **_kwargs: object) -> None:
        return None

    monkeypatch.setattr(manual_command_admission, "AsyncSessionLocal", lambda: Session())
    monkeypatch.setattr(manual_command_admission, "RedisManager", SimpleNamespace(get_instance=lambda: object()))
    monkeypatch.setattr(manual_command_admission, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(manual_command_admission, "record_hardware_command_intent", record_intent)
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_queued", mark_queued)
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_completed", mark_completed)
    async def wait_for_acks(*_args: object, **kwargs: object) -> dict[object, str]:
        calls.append("acknowledged")
        return {cast(list[object], kwargs["command_ids"])[0]: "acknowledged"}

    monkeypatch.setattr(manual_command_admission, "wait_for_hardware_command_acks", wait_for_acks)
    monkeypatch.setattr(manual_command_admission, "enqueue_request_state", _done)
    monkeypatch.setattr(manual_command_admission, "_wait_for_manual_readback", _readback_true)
    monkeypatch.setattr(manual_command_admission, "_result", result)

    _ = run_async(
        manual_command_admission._enqueue_manual(  # pyright: ignore[reportPrivateUsage]
            cast(WebSocket, object()),
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


def test_manual_command_timeout_is_reported_and_lease_released(monkeypatch: pytest.MonkeyPatch) -> None:
    captured: dict[str, object] = {}

    @final
    class Session:
        async def __aenter__(self) -> "Session":
            return self

        async def __aexit__(self, exc_type: object, exc: BaseException | None, tb: object) -> bool:
            return False

        async def commit(self) -> None:
            return None

    @final
    class Admission:
        def __init__(self, redis: object) -> None:
            return None

        async def acquire_many(self, **_kwargs: object) -> list[SimpleNamespace]:
            return [SimpleNamespace(fencing_epoch=1, lease_id="lease-1")]

        async def release(self, _lease: object) -> bool:
            return True

    async def wait_for_acks(*_args: object, **kwargs: object) -> dict[object, str]:
        return {cast(list[object], kwargs["command_ids"])[0]: "timeout"}

    async def result(*_args: object, **kwargs: object) -> None:
        captured.update(kwargs)

    monkeypatch.setattr(manual_command_admission, "AsyncSessionLocal", lambda: Session())
    monkeypatch.setattr(manual_command_admission, "RedisManager", SimpleNamespace(get_instance=lambda: object()))
    monkeypatch.setattr(manual_command_admission, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(manual_command_admission, "record_hardware_command_intent", _done)
    monkeypatch.setattr(manual_command_admission, "mark_hardware_command_intent_queued", _done)
    monkeypatch.setattr(manual_command_admission, "wait_for_hardware_command_acks", wait_for_acks)
    monkeypatch.setattr(manual_command_admission, "_result", result)

    async def sender(command_id: str) -> str:
        return command_id

    _ = run_async(
        manual_command_admission._enqueue_manual(  # pyright: ignore[reportPrivateUsage]
            cast(WebSocket, object()),
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


def test_manual_command_marks_recovery_when_acknowledged_readback_fails(monkeypatch: pytest.MonkeyPatch) -> None:
    captured: dict[str, object] = {}
    statuses: list[str] = []

    @final
    class Session:
        async def __aenter__(self) -> "Session":
            return self

        async def __aexit__(self, exc_type: object, exc: BaseException | None, tb: object) -> bool:
            return False

        async def commit(self) -> None:
            return None

    @final
    class Admission:
        def __init__(self, redis: object) -> None:
            return None

        async def acquire_many(self, **_kwargs: object) -> list[SimpleNamespace]:
            return [SimpleNamespace(fencing_epoch=1, lease_id="lease-1")]

        async def release(self, _lease: object) -> bool:
            return True

    async def mark_failure(*_args: object, **kwargs: object) -> None:
        statuses.append(cast(str, kwargs["status"]))

    async def result(*_args: object, **kwargs: object) -> None:
        captured.update(kwargs)

    async def wait_for_acks(*_args: object, **kwargs: object) -> dict[object, str]:
        return {cast(list[object], kwargs["command_ids"])[0]: "acknowledged"}

    async def readback(*_args: object, **_kwargs: object) -> bool:
        return False

    async def noop(*_args: object, **_kwargs: object) -> None:
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

    _ = run_async(
        manual_command_admission._enqueue_manual(  # pyright: ignore[reportPrivateUsage]
            cast(WebSocket, object()),
            workspace_id=7,
            channel_ids=[17],
            unit_id="unit-1",
            device_id=23,
            action="do_set",
            payload={"ch": 2, "value": 1},
            sender=_done_sender,
        )
    )

    assert statuses == ["recovery_required"]
    assert captured["reason"] == "hardware_readback_timeout"


def test_manual_command_rejects_channel_with_recovery_required_intent(monkeypatch: pytest.MonkeyPatch) -> None:
    captured: dict[str, object] = {}
    acquire_calls: list[dict[str, object]] = []

    async def blocked_channels(*_args: object, **_kwargs: object) -> set[int]:
        return {17}

    async def result(*_args: object, **kwargs: object) -> None:
        captured.update(kwargs)

    @final
    class Admission:
        def __init__(self, redis: object) -> None:
            del redis

        async def acquire_many(self, **kwargs: object) -> list[SimpleNamespace]:
            acquire_calls.append(kwargs)
            raise AssertionError("recovery barrier must run before lease admission")

    monkeypatch.setattr(manual_command_admission, "list_hardware_recovery_required_channels", blocked_channels)
    monkeypatch.setattr(manual_command_admission, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(manual_command_admission, "RedisManager", SimpleNamespace(get_instance=lambda: object()))
    monkeypatch.setattr(manual_command_admission, "_result", result)

    _ = run_async(
        manual_command_admission._enqueue_manual(  # pyright: ignore[reportPrivateUsage]
            cast(WebSocket, object()),
            workspace_id=7,
            channel_ids=[17],
            unit_id="unit-1",
            device_id=23,
            action="do_set",
            payload={"ch": 2, "value": 1},
            sender=_done_sender,
        )
    )

    assert acquire_calls == []
    assert captured == {
        "command_id": None,
        "delivery": "rejected",
        "reason": "hardware_recovery_required",
    }


async def _done(*_args: object, **_kwargs: object) -> None:
    return None


async def _done_sender(command_id: str) -> str:
    return command_id


async def _readback_true(*_args: object, **_kwargs: object) -> bool:
    return True


def test_manual_channels_reject_duplicate_scope_before_database_query(monkeypatch: pytest.MonkeyPatch) -> None:
    async def unexpected_session() -> None:
        raise AssertionError("duplicate scope must be rejected before querying channels")

    monkeypatch.setattr(manual_command_admission, "AsyncSessionLocal", unexpected_session)

    assert run_async(
        manual_command_admission._channels(  # pyright: ignore[reportPrivateUsage]
            7,
            [17, 17],
            "unit-1",
            [0, 1],
            "do",
        )
    ) is None
