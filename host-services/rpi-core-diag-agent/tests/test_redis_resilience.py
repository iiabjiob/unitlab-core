from __future__ import annotations

import asyncio
from types import SimpleNamespace

import pytest

from unitlab_rpi_core_diag_agent.agent import CoreDiagAgent
from unitlab_rpi_core_diag_agent.redis_protocol import RedisProtocol


def _config(**overrides: object) -> SimpleNamespace:
    values = {
        "redis_url": "redis://127.0.0.1:6379/0",
        "command_poll_interval_sec": 0.25,
    }
    values.update(overrides)
    return SimpleNamespace(**values)


def test_redis_client_leaves_blocking_read_without_socket_timeout(monkeypatch: pytest.MonkeyPatch) -> None:
    captured: dict[str, object] = {}

    class FakeRedis:
        pass

    def fake_from_url(url: str, **kwargs: object) -> FakeRedis:
        captured["url"] = url
        captured.update(kwargs)
        return FakeRedis()

    monkeypatch.setattr("unitlab_rpi_core_diag_agent.redis_protocol.Redis.from_url", fake_from_url)
    RedisProtocol(_config())

    assert captured["socket_connect_timeout"] == 3
    assert "socket_timeout" not in captured
    assert "retry_on_timeout" not in captured
    assert captured["health_check_interval"] == 30


def test_publish_error_does_not_kill_loop_when_redis_is_unavailable() -> None:
    asyncio.run(_assert_publish_error_is_safe())


async def _assert_publish_error_is_safe() -> None:
    agent = CoreDiagAgent(_config())

    async def fail_publish(*_args: object, **_kwargs: object) -> None:
        raise TimeoutError("Timeout reading from 127.0.0.1:6379")

    async def fake_snapshot(*_args: object, **_kwargs: object) -> None:
        return None

    agent._publish_snapshot = fake_snapshot  # type: ignore[method-assign]
    agent.redis.publish_event = fail_publish  # type: ignore[method-assign]
    await agent._publish_error_safely("command_loop_error", "redis timeout")

    assert agent._snapshot.mode == "unknown"
    assert agent._snapshot.last_error == "redis timeout"
