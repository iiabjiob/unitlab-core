from __future__ import annotations

import asyncio
from unittest.mock import AsyncMock
from typing import cast

import pytest

from app.ws.manager import WebSocketManager


@pytest.mark.anyio
async def test_disconnect_closes_transport_after_queue_overflow_cleanup() -> None:
    manager = WebSocketManager()
    websocket = AsyncMock()
    manager.active_connections.append(websocket)
    manager._outbound_queues[websocket] = asyncio.Queue[dict[str, object]](maxsize=1)  # pyright: ignore[reportPrivateUsage]
    manager._sender_tasks[websocket] = asyncio.create_task(asyncio.sleep(60))  # pyright: ignore[reportPrivateUsage]

    manager.disconnect(websocket, reason="queue_full")
    await asyncio.sleep(0)
    await asyncio.sleep(0)

    websocket.close.assert_awaited_once()  # pyright: ignore[reportAny]
    assert websocket not in manager.active_connections
    assert websocket not in manager._outbound_queues  # pyright: ignore[reportPrivateUsage]
    counters = cast(dict[str, int], manager.get_stats()["counters"])
    assert counters["disconnect.reason.queue_full"] == 1


@pytest.mark.anyio
async def test_sender_timeout_closes_transport_and_removes_runtime_state() -> None:
    manager = WebSocketManager()
    websocket = AsyncMock()
    websocket.send_json.side_effect = asyncio.TimeoutError  # pyright: ignore[reportAny]
    manager.active_connections.append(websocket)
    queue: asyncio.Queue[dict[str, object]] = asyncio.Queue(maxsize=1)
    manager._outbound_queues[websocket] = queue  # pyright: ignore[reportPrivateUsage]
    await queue.put({"event": "slow"})

    task = asyncio.create_task(manager._sender_loop(websocket, queue))  # pyright: ignore[reportPrivateUsage]
    await asyncio.sleep(0)
    await asyncio.sleep(0)
    await asyncio.sleep(0)

    assert task.done()
    websocket.close.assert_awaited_once()  # pyright: ignore[reportAny]
    assert websocket not in manager.active_connections
    assert websocket not in manager._outbound_queues  # pyright: ignore[reportPrivateUsage]
    counters = cast(dict[str, int], manager.get_stats()["counters"])
    assert counters["disconnect.reason.send_timeout"] == 1
