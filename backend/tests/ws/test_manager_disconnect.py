from __future__ import annotations

import asyncio
from unittest.mock import AsyncMock

import pytest

from app.ws.manager import WebSocketManager


@pytest.mark.anyio
async def test_disconnect_closes_transport_after_queue_overflow_cleanup() -> None:
    manager = WebSocketManager()
    websocket = AsyncMock()
    manager.active_connections.append(websocket)
    manager._outbound_queues[websocket] = asyncio.Queue(maxsize=1)
    manager._sender_tasks[websocket] = asyncio.create_task(asyncio.sleep(60))

    manager.disconnect(websocket, reason="queue_full")
    await asyncio.sleep(0)
    await asyncio.sleep(0)

    websocket.close.assert_awaited_once()
    assert websocket not in manager.active_connections
    assert websocket not in manager._outbound_queues
    assert manager.get_stats()["counters"]["disconnect.reason.queue_full"] == 1
