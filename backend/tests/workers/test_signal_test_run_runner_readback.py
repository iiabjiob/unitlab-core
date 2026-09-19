from __future__ import annotations

import asyncio
from collections.abc import Awaitable
from typing import TypeVar, cast

from app.workers.signal_test_run_runner import SignalTestRunRedisClient, _wait_for_bit_readback  # pyright: ignore[reportPrivateUsage]


T = TypeVar("T")


class ReadbackRedis:
    def __init__(self, value: object) -> None:
        self.value: object = value

    async def get(self, key: str):
        _ = key
        return self.value


def run_async(awaitable: Awaitable[T]) -> T:
    return asyncio.run(awaitable)


def test_bit_readback_does_not_treat_missing_state_as_zero() -> None:
    assert run_async(
        _wait_for_bit_readback(
            cast(SignalTestRunRedisClient, cast(object, ReadbackRedis(None))),
            unit_id="unit-1",
            channel_index=0,
            expected_value=0,
            timeout_ms=100,
        )
    ) is False


def test_bit_readback_accepts_explicit_zero_state() -> None:
    assert run_async(
        _wait_for_bit_readback(
            cast(SignalTestRunRedisClient, cast(object, ReadbackRedis("0"))),
            unit_id="unit-1",
            channel_index=0,
            expected_value=0,
            timeout_ms=100,
        )
    ) is True
