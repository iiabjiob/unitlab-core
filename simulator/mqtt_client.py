"""Async MQTT utilities and device base class for the UnitLab simulator."""

from __future__ import annotations

import asyncio
import json
import random
import time
import signal
import contextlib
from dataclasses import dataclass
from typing import Awaitable, Callable, Optional

from gmqtt import Client as GMQTTClient

from simulator.packet_structures import (
    PacketBuilder,
    PacketHeader,
    RegisterFrame,
    RespError,
    RespFrame,
    RespStatus,
    Sys,
    build_packet,
    encode_resp,
    encode_register,
    parse_packet,
)
from simulator.utils.logger import get_logger
from simulator.utils import binary_tools

OnMessageHook = Callable[[str, bytes], Awaitable[None]]


_HEARTBEAT_MIN_INTERVAL = 0.5
_HEARTBEAT_DIAG_INTERVAL = 60.0
_FLAKY_DEVICE_RATIO = 0.05
_FLAKY_HEARTBEAT_OUTAGE_PROB = 0.15
_FLAKY_HEARTBEAT_BACKOFF_RANGE = (2.0, 10.0)
_FLAKY_EXCHANGE_PROB = 0.2
_FLAKY_ERROR_SHARE = 0.6
_RECONNECT_BACKOFF_INITIAL = 1.0
_RECONNECT_BACKOFF_MAX = 30.0


@dataclass(slots=True)
class BrokerSettings:
    """MQTT broker configuration derived from YAML."""

    host: str
    port: int
    keepalive: int
    username: Optional[str] = None
    password: Optional[str] = None


@dataclass(slots=True)
class BehaviorSettings:
    """Runtime behaviour knobs shared by all simulated devices."""

    randomize: bool
    heartbeat: float
    reconnect_chance: float
    packet_loss: float
    flaky_device_ratio: float = _FLAKY_DEVICE_RATIO
    flaky_exchange_prob: float = _FLAKY_EXCHANGE_PROB
    command_error_rate: float = 0.05


class SimulatorMQTTClient:
    """Thin wrapper around gmqtt that exposes asyncio-friendly hooks."""

    def __init__(self, client_id: str) -> None:
        self._client = GMQTTClient(client_id)
        self._logger = get_logger(f"mqtt.{client_id}")

        self._on_message: Optional[OnMessageHook] = None
        self._on_connect: Optional[Callable[[], None]] = None
        self._on_disconnect: Optional[Callable[[Optional[BaseException]], None]] = None

        self._connected = asyncio.Event()
        self._lock = asyncio.Lock()

        # Bind gmqtt callbacks
        self._client.on_connect = self._handle_connect
        self._client.on_disconnect = self._handle_disconnect
        self._client.on_message = self._handle_message
        self._client.on_subscribe = self._handle_subscribe

    def set_handlers(
        self,
        *,
        on_message: Optional[OnMessageHook] = None,
        on_connect: Optional[Callable[[], None]] = None,
        on_disconnect: Optional[Callable[[Optional[BaseException]], None]] = None,
    ) -> None:
        self._on_message = on_message
        self._on_connect = on_connect
        self._on_disconnect = on_disconnect

    async def connect(self, broker: BrokerSettings) -> None:
        async with self._lock:
            if self._connected.is_set():
                return
            if broker.username:
                self._client.set_auth_credentials(broker.username, broker.password or "")
            await self._client.connect(
                broker.host,
                broker.port,
                keepalive=broker.keepalive,
            )
            await self._connected.wait()

    async def disconnect(self) -> None:
        async with self._lock:
            if not self._connected.is_set():
                return
            with contextlib.suppress(BrokenPipeError, ConnectionResetError, OSError):
                await self._client.disconnect()
            self._connected.clear()

    async def reconnect(self, broker: BrokerSettings) -> None:
        await self.disconnect()
        await asyncio.sleep(0.1)
        await self.connect(broker)

    async def wait_connected(self) -> None:
        await self._connected.wait()

    @property
    def is_connected(self) -> bool:
        return self._connected.is_set()

    def subscribe(self, topic: str, *, qos: int = 0) -> None:
        if not topic:
            raise ValueError("Topic must be a non-empty string")
        self._client.subscribe(topic, qos)

    def publish(self, topic: str, payload: bytes, *, qos: int = 0, retain: bool = False) -> None:
        if not topic:
            raise ValueError("Topic must be provided")
        try:
            self._client.publish(topic, payload, qos=qos, retain=retain)
        except (BrokenPipeError, ConnectionResetError, OSError) as exc:
            self._logger.warning("Publish failed on %s: %s", topic, exc)
            self._connected.clear()
            if self._on_disconnect:
                self._on_disconnect(exc)

    # --- gmqtt callbacks -------------------------------------------------
    def _handle_connect(self, client, flags, rc, properties):  # pragma: no cover - gmqtt callback
        self._logger.debug("Connected to MQTT broker (%s)", rc)
        self._connected.set()
        if self._on_connect:
            self._on_connect()

    def _handle_disconnect(self, client, packet, exc=None):  # pragma: no cover - gmqtt callback
        self._logger.debug("Disconnected from MQTT broker")
        self._connected.clear()
        if self._on_disconnect:
            self._on_disconnect(exc)

    def _handle_subscribe(self, client, mid, qos, properties):  # pragma: no cover - gmqtt callback
        self._logger.debug("Subscribed mid=%s qos=%s", mid, qos)

    def _handle_message(self, client, topic, payload, qos, properties):  # pragma: no cover - gmqtt callback
        if self._on_message:
            asyncio.create_task(self._on_message(topic, payload))
        else:
            self._logger.debug("Dropped message from %s (no handler)", topic)


# ---------------------------------------------------------------------------
# Topic helpers (mirrors backend/app/infrastructure/mqtt/topics.py)
# ---------------------------------------------------------------------------

def topic_register(unit_id: str) -> str:
    return f"{unit_id}/reg"


def topic_heartbeat(unit_id: str) -> str:
    return f"{unit_id}/h"


def topic_heartbeat_diag(unit_id: str) -> str:
    return f"{unit_id}/hd"


def topic_state(unit_id: str) -> str:
    return f"{unit_id}/s"


def topic_cmd(unit_id: str) -> str:
    return f"{unit_id}/c"


def topic_req_state(unit_id: str) -> str:
    return f"{unit_id}/q"


def topic_resp(unit_id: str) -> str:
    return f"{unit_id}/r"


def topic_info(unit_id: str) -> str:
    return f"{unit_id}/i"


class SimulatedDeviceBase:
    """Reusable asyncio device skeleton used by DO/DI/AO simulators."""

    STARTUP_JITTER = 1.5

    def __init__(
        self,
        *,
        unit_id: str,
        signals: int,
        interval: float,
        broker: BrokerSettings,
        behavior: BehaviorSettings,
        test_mode: bool = False,
    ) -> None:
        if interval <= 0:
            raise ValueError("Interval must be positive")
        self.unit_id = unit_id
        self.signals = signals
        self.interval = interval
        self.broker = broker
        self.behavior = behavior
        self.test_mode = test_mode

        self._mqtt = SimulatorMQTTClient(unit_id)
        self._mqtt.set_handlers(
            on_message=self._on_message,
            on_connect=self._on_connect,
            on_disconnect=self._on_disconnect,
        )

        self._logger = get_logger(f"device.{unit_id}")
        self._builder = PacketBuilder()
        self._rng = random.Random(hash(unit_id) & 0xFFFFFFFF)
        self._stop_event = asyncio.Event()
        self._connected = asyncio.Event()
        self._tasks: list[asyncio.Task[None]] = []
        self._aux_tasks: set[asyncio.Task[None]] = set()
        self._lock = asyncio.Lock()
        self._reconnect_task: Optional[asyncio.Task[None]] = None
        self._suspend_auto_reconnect = False
        self._is_flaky_device = self._rng.random() < self.behavior.flaky_device_ratio
        heartbeat_base = max(self.behavior.heartbeat, _HEARTBEAT_MIN_INTERVAL)
        self._heartbeat_offset = self._rng.uniform(0.0, heartbeat_base)
        self._heartbeat_started_at = time.monotonic()
        self._heartbeat_fast_seq = 0
        self._heartbeat_diag_seq = 0
        self._heartbeat_diag_last_sent_at: float | None = None

    # --- lifecycle ------------------------------------------------------
    async def run(self) -> None:
        try:
            await asyncio.sleep(self._rng.random() * self.STARTUP_JITTER)
            await self._connect_and_bootstrap()
            main_task = asyncio.create_task(self._state_loop(), name=f"state:{self.unit_id}")
            hb_task = asyncio.create_task(self._heartbeat_loop(), name=f"heartbeat:{self.unit_id}")
            self._tasks.extend([main_task, hb_task])
            await self._stop_event.wait()
        except asyncio.CancelledError:  # pragma: no cover - cooperative cancellation
            raise
        finally:
            await self._shutdown()

    async def stop(self) -> None:
        self._stop_event.set()

    async def _shutdown(self) -> None:
        self._suspend_auto_reconnect = True
        if self._reconnect_task:
            self._reconnect_task.cancel()
            await asyncio.gather(self._reconnect_task, return_exceptions=True)
            self._reconnect_task = None
        for task in self._tasks:
            task.cancel()
        await asyncio.gather(*self._tasks, return_exceptions=True)
        for task in list(self._aux_tasks):
            task.cancel()
        await asyncio.gather(*self._aux_tasks, return_exceptions=True)
        await self._mqtt.disconnect()
        self._tasks.clear()
        self._aux_tasks.clear()

    async def _connect_and_bootstrap(self) -> None:
        await self._mqtt.connect(self.broker)
        await self._mqtt.wait_connected()
        await self._post_connect()

    async def _post_connect(self) -> None:
        self._connected.set()
        self._mqtt.subscribe(topic_cmd(self.unit_id))
        self._mqtt.subscribe(topic_req_state(self.unit_id))
        self._mqtt.subscribe(topic_info(self.unit_id))
        await self._send_registration()

    # --- gmqtt hooks ----------------------------------------------------
    def _on_connect(self) -> None:  # pragma: no cover - event hook
        self._connected.set()
        if self._reconnect_task:
            self._reconnect_task.cancel()
            self._reconnect_task = None

    def _on_disconnect(self, exc: Optional[BaseException]) -> None:  # pragma: no cover - event hook
        self._connected.clear()
        if self._stop_event.is_set() or self._suspend_auto_reconnect:
            return
        if self._reconnect_task and not self._reconnect_task.done():
            return
        self._reconnect_task = asyncio.create_task(
            self._restore_connection_loop(),
            name=f"reconnect:{self.unit_id}",
        )

    async def _on_message(self, topic: str, payload: bytes) -> None:
        if not payload:
            self._logger.debug("Received empty payload from %s", topic)
            return
        try:
            header, body = parse_packet(payload)
        except ValueError as exc:
            self._logger.warning("Failed to parse packet from %s: %s", topic, exc)
            return
        await self.handle_packet(topic, header, body)

    # --- core loops -----------------------------------------------------
    async def _state_loop(self) -> None:
        try:
            while not self._stop_event.is_set():
                if not self._connected.is_set():
                    await asyncio.sleep(0.5)
                    continue

                base_interval = max(self.interval, 0.1)
                if self._is_flaky_device and self._rng.random() < 0.08:
                    stall = base_interval * self._rng.uniform(4.0, 9.0)
                    self._logger.debug(
                        "Suspending state updates for %.2fs to emulate stall",
                        stall,
                    )
                    await asyncio.sleep(stall)
                    continue

                should_auto_publish = self.should_auto_publish()

                if should_auto_publish and self.behavior.randomize:
                    self.randomize_state()

                if should_auto_publish:
                    await self.publish_state()

                if self._rng.random() < self.behavior.reconnect_chance:
                    await self._simulate_reconnect()

                jitter = self._rng.uniform(-0.2, 0.5) * base_interval
                await asyncio.sleep(max(0.1, base_interval + jitter))
        except asyncio.CancelledError:  # pragma: no cover - cooperative cancellation
            pass

    async def _heartbeat_loop(self) -> None:
        try:
            interval = max(self.behavior.heartbeat, _HEARTBEAT_MIN_INTERVAL)
            await asyncio.sleep(self._heartbeat_offset)
            while not self._stop_event.is_set():
                if not self._connected.is_set():
                    await asyncio.sleep(min(interval, 1.0))
                    continue
                if self._is_flaky_device and self._rng.random() < _FLAKY_HEARTBEAT_OUTAGE_PROB:
                    low, high = _FLAKY_HEARTBEAT_BACKOFF_RANGE
                    backoff = interval * self._rng.uniform(low, high)
                    self._logger.debug(
                        "Skipping heartbeat for %.2fs to emulate outage", backoff
                    )
                    await asyncio.sleep(backoff)
                    continue
                await self._publish_heartbeat_telemetry()
                jitter = self._rng.uniform(-0.25, 0.35) * interval
                await asyncio.sleep(max(_HEARTBEAT_MIN_INTERVAL, interval + jitter))
        except asyncio.CancelledError:  # pragma: no cover - cooperative cancellation
            pass

    async def _publish_heartbeat_telemetry(self) -> None:
        uptime_s = max(0, int(time.monotonic() - self._heartbeat_started_at))

        self._heartbeat_fast_seq += 1
        fast_payload = {
            "kind": "fast",
            "seq": self._heartbeat_fast_seq,
            "state": 1,
            "degraded": 0,
            "faults": 0,
            "up_s": uptime_s,
            "mqtt": 1 if self._connected.is_set() else 0,
            "mem": {"free": 180000 + self._rng.randint(-5000, 5000)},
            "last_comp": "SIM",
            "last_reason": "ok",
            "last_ms": uptime_s * 1000,
        }
        self._mqtt.publish(topic_heartbeat(self.unit_id), json.dumps(fast_payload, separators=(",", ":")).encode("utf-8"))

        now = time.monotonic()
        if self._heartbeat_diag_last_sent_at is None or (now - self._heartbeat_diag_last_sent_at) >= _HEARTBEAT_DIAG_INTERVAL:
            self._heartbeat_diag_seq += 1
            diag_mem_free = 180000 + self._rng.randint(-5000, 5000)
            diag_mem_min = 170000 + self._rng.randint(-5000, 5000)
            diag_mem_largest = 90000 + self._rng.randint(-4000, 4000)
            mem_stab_ready = 1 if uptime_s >= 30 else 0
            mem_stab_base_ms = 30000 if mem_stab_ready else 0
            mem_stab_base_free = 184000 if mem_stab_ready else 0
            mem_stab_base_largest = 90000 if mem_stab_ready else 0

            diag_payload = {
                "kind": "diag",
                "seq": self._heartbeat_diag_seq,
                "state": 1,
                "degraded": 0,
                "faults": 0,
                "up_s": uptime_s,
                "last_comp": "SIM",
                "last_reason": "ok",
                "last_ms": uptime_s * 1000,
                "proto": {
                    "short": 0,
                    "payload": 0,
                    "truncated": 0,
                    "version": 0,
                    "no_handler": 0,
                    "dispatched": 0,
                },
                "mqtt_in": {
                    "enq": 0,
                    "disp": 0,
                    "ovf": 0,
                    "large": 0,
                    "noq": 0,
                    "rlim": 0,
                    "rl_scan": 0,
                    "rl_req": 0,
                    "rl_cmd": 0,
                    "depth": 0,
                    "depth_max": 0,
                },
                "mem": {
                    "free": diag_mem_free,
                    "min": diag_mem_min,
                    "largest": diag_mem_largest,
                },
                "mem_stab": {
                    "ready": mem_stab_ready,
                    "base_ms": mem_stab_base_ms,
                    "base_free": mem_stab_base_free,
                    "base_largest": mem_stab_base_largest,
                    "max_d_free": abs(diag_mem_free - mem_stab_base_free) if mem_stab_ready else 0,
                    "max_d_largest": abs(diag_mem_largest - mem_stab_base_largest) if mem_stab_ready else 0,
                    "chg": max(0, int((uptime_s - 30) // 60)) if mem_stab_ready else 0,
                    "samples": max(0, int((uptime_s - 30) // 5) + 1) if mem_stab_ready else 0,
                },
                "alloc": {
                    "boot_done": 1 if uptime_s >= 5 else 0,
                    "boot_ms": 5000 if uptime_s >= 5 else 0,
                    "boot_new": 24,
                    "boot_del": 19,
                    "run_new": max(0, int(uptime_s // 90)),
                    "run_del": max(0, int(uptime_s // 95)),
                    "new_fail": 0,
                },
                "reset": {
                    "code": 1,
                    "label": "simulator",
                    "boot": 1,
                    "hist": 1,
                },
                "tasks": {"stale": 0},
                "stack": [
                    {"task": "StateTask", "min_words": 1024, "last_seen_ms": uptime_s * 1000},
                    {"task": "HeartbeatTask", "min_words": 768, "last_seen_ms": uptime_s * 1000},
                ],
            }
            self._mqtt.publish(topic_heartbeat_diag(self.unit_id), json.dumps(diag_payload, separators=(",", ":")).encode("utf-8"))
            self._heartbeat_diag_last_sent_at = now

    async def _simulate_reconnect(self) -> None:
        async with self._lock:
            if not self._mqtt.is_connected:
                return
            self._logger.debug("Simulating reconnect")
            self._suspend_auto_reconnect = True
            try:
                await self._mqtt.disconnect()
                await asyncio.sleep(0.2 + self._rng.random())
                await self._mqtt.connect(self.broker)
                await self._mqtt.wait_connected()
                await self._post_connect()
            except Exception:
                self._logger.warning("Simulated reconnect failed; scheduling recovery loop", exc_info=True)
                if not self._stop_event.is_set():
                    if not self._reconnect_task or self._reconnect_task.done():
                        self._reconnect_task = asyncio.create_task(
                            self._restore_connection_loop(),
                            name=f"reconnect:{self.unit_id}",
                        )
            finally:
                self._suspend_auto_reconnect = False

    async def _restore_connection_loop(self) -> None:
        backoff = _RECONNECT_BACKOFF_INITIAL
        while not self._stop_event.is_set():
            try:
                await self._mqtt.connect(self.broker)
                await self._mqtt.wait_connected()
                await self._post_connect()
                self._logger.info("MQTT reconnect succeeded")
                return
            except asyncio.CancelledError:  # pragma: no cover - cooperative cancellation
                raise
            except Exception as exc:
                self._logger.warning(
                    "MQTT reconnect failed (%s); retry in %.1fs",
                    type(exc).__name__,
                    backoff,
                )
                await asyncio.sleep(backoff + self._rng.uniform(0.0, 0.5))
                backoff = min(_RECONNECT_BACKOFF_MAX, backoff * 2)

    # --- public helpers -------------------------------------------------
    async def publish_state(self, *, packet_id: Optional[int] = None) -> None:
        raise NotImplementedError

    def randomize_state(self) -> None:
        raise NotImplementedError

    def should_auto_publish(self) -> bool:
        """Return True when the periodic loop should emit state frames."""
        return True

    async def handle_packet(self, topic: str, header: PacketHeader, payload: bytes) -> None:
        mode = header.mode

        # 1) REGISTER REQUESTED VIA SCAN
        if mode == Sys.SCAN:
            self._logger.debug("SCAN received → sending REGISTER + STATE for %s", self.unit_id)

            # re-send REGISTER
            await self._send_registration()

            # send current state snapshot
            await self.publish_state(packet_id=header.packet_id)
            return

        # 3) CMD — delegated to subclass
        raise NotImplementedError("handle_packet must be implemented in subclass")

    # --- shared utilities -----------------------------------------------
    async def _publish_packet(
        self,
        topic: str,
        mode: int,
        payload: bytes,
        *,
        packet_id: Optional[int] = None,
        retain: bool = False,
    ) -> None:
        if self.behavior.packet_loss and self._rng.random() < self.behavior.packet_loss:
            self._logger.debug("Dropping packet on %s to simulate packet loss", topic)
            return
        frame = build_packet(
            mode,
            payload,
            builder=self._builder,
            packet_id=packet_id,
        )
        if self.test_mode:
            dump = binary_tools.hex_dump(frame)
            self._logger.info("TX %s\n%s", topic, dump)
        self._mqtt.publish(topic, frame, retain=retain)

    async def _send_registration(self) -> None:
        payload = encode_register(
            self._build_register_frame()
        )
        await self._publish_packet(topic_register(self.unit_id), Sys.REGISTER, payload)

    async def _send_resp(
        self,
        status: RespStatus,
        error: RespError = RespError.NONE,
        *,
        packet_id: Optional[int] = None,
    ) -> None:
        payload = encode_resp(RespFrame(status=status, err_code=error))
        await self._publish_packet(
            topic_resp(self.unit_id),
            Sys.RESP,
            payload,
            packet_id=packet_id,
        )

    def _build_register_frame(self) -> RegisterFrame:
        return RegisterFrame(
            type_code=self.device_type_code,
            unit_id=self.unit_id,
            fw_version=self.firmware_version,
            num_channels=self.signals,
        )

    def _fault_decision(self) -> Optional[str]:
        if not self._is_flaky_device:
            return None
        if self._rng.random() >= self.behavior.flaky_exchange_prob:
            return None
        return "error" if self._rng.random() < _FLAKY_ERROR_SHARE else "drop"

    async def _maybe_fail_exchange(self, packet_id: int, *, context: str) -> Optional[str]:
        decision = self._fault_decision()
        if decision == "error":
            await self._send_resp(
                RespStatus.INTERNAL_ERROR,
                RespError.HW_FAILURE,
                packet_id=packet_id,
            )
        if decision:
            self._logger.debug(
                "Simulating %s failure (%s) for packet 0x%04X",
                context,
                decision,
                packet_id & 0xFFFF,
            )
        return decision

    @property
    def device_type_code(self) -> str:
        raise NotImplementedError

    @property
    def firmware_version(self) -> int:
        return 0x0102

    def _register_aux_task(self, task: asyncio.Task[None]) -> None:
        self._aux_tasks.add(task)
        task.add_done_callback(self._aux_tasks.discard)


def install_signal_handlers(stop_event: asyncio.Event) -> None:
    """Bind SIGINT/SIGTERM handlers so ``Ctrl+C`` shuts down gracefully."""
    loop = asyncio.get_event_loop()

    def _signal_handler() -> None:
        stop_event.set()

    for sig in (signal.SIGINT, signal.SIGTERM):
        with contextlib.suppress(NotImplementedError):  # pragma: no cover - Windows
            loop.add_signal_handler(sig, _signal_handler)
