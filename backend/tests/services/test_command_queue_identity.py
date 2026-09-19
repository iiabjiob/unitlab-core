from __future__ import annotations

import pytest

from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.protocol.modes import Cmd
from app.services import command_queue_service
from app.core.mqtt_dto import OutboundCmdMsg


_captured_outbound: list[OutboundCmdMsg] = []


async def _capture_outbound(message: OutboundCmdMsg) -> str:
    _captured_outbound.append(message)
    return "stream-1"


@pytest.mark.anyio
async def test_do_command_assigns_distinct_command_id(monkeypatch: pytest.MonkeyPatch) -> None:
    _captured_outbound.clear()
    monkeypatch.setattr(command_queue_service, "enqueue_outbound_command", _capture_outbound)

    class FakeRedis:
        async def set(self, _key: str, _value: object, **_kwargs: object) -> bool:
            return True

    def get_instance(_cls: type[RedisManager]) -> FakeRedis:
        return FakeRedis()

    monkeypatch.setattr(RedisManager, "get_instance", classmethod(get_instance))

    command_id = await command_queue_service.enqueue_do_command(
        unit_id="UNIT-1",
        mode=Cmd.SET_SINGLE_BIT,
        ch=2,
        value=1,
        correlation_id="test-correlation",
    )

    assert command_id
    message = _captured_outbound[0]
    assert message.command_id == command_id
    assert message.packet_id is not None
    assert message.correlation_id == "test-correlation"


@pytest.mark.anyio
async def test_command_packet_allocator_skips_reserved_packet_id(monkeypatch: pytest.MonkeyPatch) -> None:
    class FakeRedis:
        def __init__(self) -> None:
            self.values: dict[str, str] = {"hardware:command:UNIT-1:10": "old-command"}

        async def set(self, key: str, value: str, **kwargs: object) -> bool:
            if kwargs.get("nx") and key in self.values:
                return False
            self.values[key] = value
            return True

    redis = FakeRedis()
    packet_ids = iter((10, 11))
    monkeypatch.setattr(command_queue_service, "next_packet_id", lambda: next(packet_ids))
    def get_instance(_cls: type[RedisManager]) -> FakeRedis:
        return redis

    monkeypatch.setattr(RedisManager, "get_instance", classmethod(get_instance))

    packet_id, reserved = await command_queue_service._allocate_command_packet("UNIT-1", "new-command")  # pyright: ignore[reportPrivateUsage]

    assert (packet_id, reserved) == (11, True)
    assert redis.values["hardware:command:UNIT-1:11"] == "new-command"
