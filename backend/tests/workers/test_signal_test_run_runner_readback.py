from __future__ import annotations

import asyncio

from app.workers.signal_test_run_runner import _wait_for_bit_readback


class ReadbackRedis:
    def __init__(self, value) -> None:
        self.value = value

    async def get(self, key: str):
        return self.value


def run_async(awaitable):
    return asyncio.run(awaitable)


def test_bit_readback_does_not_treat_missing_state_as_zero() -> None:
    assert run_async(
        _wait_for_bit_readback(
            ReadbackRedis(None),
            unit_id="unit-1",
            channel_index=0,
            expected_value=0,
            timeout_ms=100,
        )
    ) is False


def test_bit_readback_accepts_explicit_zero_state() -> None:
    assert run_async(
        _wait_for_bit_readback(
            ReadbackRedis("0"),
            unit_id="unit-1",
            channel_index=0,
            expected_value=0,
            timeout_ms=100,
        )
    ) is True
