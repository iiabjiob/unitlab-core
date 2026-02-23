import asyncio
import time
from collections import defaultdict
from typing import List, Union, Dict, Any
from pydantic import BaseModel
from fastapi import WebSocket
from app.core.logger import get_logger
from app.core.config import get_settings

logger = get_logger("ws")
settings = get_settings()

class WebSocketManager:
    _instance = None

    def __init__(self):
        self.active_connections: List[WebSocket] = []
        self._outbound_queues: dict[WebSocket, asyncio.Queue[Dict[str, Any]]] = {}
        self._sender_tasks: dict[WebSocket, asyncio.Task[None]] = {}
        self._sync_tasks: dict[WebSocket, asyncio.Task[None]] = {}
        self._counters: dict[str, int] = defaultdict(int)

    @classmethod
    def get_instance(cls):
        if cls._instance is None:
            cls._instance = cls()
        return cls._instance

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        self._inc("connect.calls")
        queue_size = max(int(settings.ws_outbound_queue_size or 2048), 16)
        queue: asyncio.Queue[Dict[str, Any]] = asyncio.Queue(maxsize=queue_size)
        self._outbound_queues[websocket] = queue
        self._sender_tasks[websocket] = asyncio.create_task(self._sender_loop(websocket, queue))
        self._sync_tasks[websocket] = asyncio.create_task(self._run_initial_sync(websocket))
        logger.debug(
            "WS connect accepted: active=%d queues=%d sender_tasks=%d",
            len(self.active_connections),
            len(self._outbound_queues),
            len(self._sender_tasks),
        )

    def disconnect(self, websocket: WebSocket, *, reason: str = "unknown"):
        removed = False
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
            removed = True
        self._outbound_queues.pop(websocket, None)
        sender_task = self._sender_tasks.pop(websocket, None)
        if sender_task is not None:
            sender_task.cancel()
        sync_task = self._sync_tasks.pop(websocket, None)
        if sync_task is not None:
            sync_task.cancel()
        if removed:
            self._inc("disconnect.calls")
            self._inc(f"disconnect.reason.{reason}")
            logger.debug(
                "WS disconnect: reason=%s active=%d queues=%d sender_tasks=%d",
                reason,
                len(self.active_connections),
                len(self._outbound_queues),
                len(self._sender_tasks),
            )

    async def send_event(self, websocket: WebSocket, event: Union[BaseModel, Dict[str, Any]]):
        """Send a Pydantic or dict event to a specific client."""
        payload, _ = self._serialize_event(event)
        await self._enqueue_payload(websocket, payload, log_send=True)

    async def broadcast(self, event: Union[BaseModel, Dict[str, Any]]):
        """Broadcast a Pydantic or dict event to all connected clients."""
        sockets = list(self.active_connections)
        if not sockets:
            return
        self._inc("broadcast.calls")

        payload, channel = self._serialize_event(event)
        start_total = time.perf_counter()

        results = await asyncio.gather(
            *(self._enqueue_payload(websocket, payload, log_send=False) for websocket in sockets),
            return_exceptions=True,
        )

        sent = 0
        failed = 0
        for result in results:
            if isinstance(result, Exception):
                failed += 1
                self._inc("broadcast.enqueue.exception")
                logger.warning(f"⚠️ Failed to broadcast to one client: {result}")
                continue
            if result:
                sent += 1
            else:
                failed += 1
                self._inc("broadcast.enqueue.failed")

        total_latency = (time.perf_counter() - start_total) * 1000
        self._inc("broadcast.clients.total", amount=len(sockets))
        self._inc("broadcast.clients.sent", amount=sent)
        self._inc("broadcast.clients.failed", amount=failed)
        logger.debug(
            f"📡 Broadcast channel={channel} clients={len(sockets)} sent={sent} "
            f"failed={failed} total={total_latency:.1f} ms"
        )

    def _serialize_event(self, event: Union[BaseModel, Dict[str, Any]]) -> tuple[Dict[str, Any], str | None]:
        if isinstance(event, BaseModel):
            payload = event.model_dump(mode="json")
            channel = getattr(event, "channel", None)
        else:
            payload = event
            channel = event.get("channel")
        return payload, channel

    async def _enqueue_payload(self, websocket: WebSocket, payload: Dict[str, Any], *, log_send: bool) -> bool:
        queue = self._outbound_queues.get(websocket)
        if queue is None:
            return False
        try:
            if log_send:
                logger.debug(f"➡️ Sending WS event: {payload}")
            queue.put_nowait(payload)
            return True
        except asyncio.QueueFull:
            logger.warning("⚠️ WS outbound queue full, disconnecting slow client")
            self._inc("send.queue_full")
            self.disconnect(websocket, reason="queue_full")
            return False

    async def _sender_loop(self, websocket: WebSocket, queue: asyncio.Queue[Dict[str, Any]]) -> None:
        timeout_s = max(settings.ws_send_timeout_ms, 100) / 1000
        try:
            while True:
                payload = await queue.get()
                try:
                    await asyncio.wait_for(websocket.send_json(payload), timeout=timeout_s)
                finally:
                    queue.task_done()
        except asyncio.CancelledError:
            raise
        except asyncio.TimeoutError:
            logger.warning("⚠️ WS send timeout in sender loop, disconnecting client")
            self._inc("send.timeout")
            self.disconnect(websocket, reason="send_timeout")
        except RuntimeError as e:
            logger.warning(f"⚠️ Failed to send to WS client: {e}")
            self._inc("send.runtime_error")
            self.disconnect(websocket, reason="runtime_error")
        except Exception as e:
            logger.warning(f"⚠️ Unexpected WS send error: {e}")
            self._inc("send.unexpected_error")
            self.disconnect(websocket, reason="unexpected_error")

    async def _run_initial_sync(self, websocket: WebSocket) -> None:
        try:
            # Send stored states after connect without blocking the accept path.
            from app.services.ws_state_service import WsStateService

            await WsStateService.sync_client(websocket)
        except asyncio.CancelledError:
            raise
        except Exception as e:
            logger.warning(f"⚠️ WS initial sync failed: {e}")
            self._inc("sync.initial.failed")
            self.disconnect(websocket, reason="initial_sync_failed")
        finally:
            task = self._sync_tasks.get(websocket)
            current = asyncio.current_task()
            if task is not None and task is current:
                self._sync_tasks.pop(websocket, None)

    def get_stats(self) -> dict[str, Any]:
        return {
            "active_connections": len(self.active_connections),
            "outbound_queues": len(self._outbound_queues),
            "sender_tasks": len(self._sender_tasks),
            "sync_tasks": len(self._sync_tasks),
            "counters": dict(self._counters),
        }

    def reset_stats(self) -> dict[str, Any]:
        self._counters.clear()
        return self.get_stats()

    def _inc(self, key: str, *, amount: int = 1) -> None:
        self._counters[key] += int(amount)
