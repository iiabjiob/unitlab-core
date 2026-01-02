"""Digital input (DI) device simulator."""

from __future__ import annotations

import asyncio

from typing import TYPE_CHECKING, Optional

from simulator.mqtt_client import (
    SimulatedDeviceBase,
    topic_state,
)
from simulator.packet_structures import (
    Cmd,
    Mode,
    RespError,
    RespStatus,
    StateAllBit,
    StateSingleBit,
    StateChangedBit,
    DiagAllDi,
    StateLatchedBit,
    encode_state_all_bit,
    encode_state_single_bit,
    encode_state_changed_bit,
    encode_state_diag_di,
    encode_state_latched_bit,
)


if TYPE_CHECKING:  # pragma: no cover - hints only
    from simulator.mqtt_client import BehaviorSettings, BrokerSettings


_DI_EXTERNAL_EVENT_PROB = 0.18
_DI_PULSE_DURATION_RANGE = (0.05, 0.6)
_DI_LATCH_DURATION_RANGE = (4.0, 12.0)


class SimulatedDIDevice(SimulatedDeviceBase):
    """Emulates a read-only ESP32 reporting digital inputs."""

    def __init__(
        self,
        *,
        unit_id: str,
        signals: int,
        interval: float,
        broker: "BrokerSettings",
        behavior: "BehaviorSettings",
        test_mode: bool = False,
    ) -> None:
        super().__init__(
            unit_id=unit_id,
            signals=signals,
            interval=interval,
            broker=broker,
            behavior=behavior,
            test_mode=test_mode,
        )
        self._bitmask = self._rng.getrandbits(max(signals, 1))
        self._lock = asyncio.Lock()
        self._diag_lock = asyncio.Lock()
        self._diag_seen = 0
        self._diag_stuck = 0
        self._diag_lost = 0
        self._latched_mask = 0
        self._diag_latched_delta = 0
        self._diag_latched_cause = 0

    @property
    def device_type_code(self) -> str:
        return "DI  "

    def randomize_state(self) -> None:
        if self.signals <= 0:
            return
        ch = self._rng.randrange(self.signals)
        current = (self._bitmask >> ch) & 0x01
        next_value = 0 if current else 1
        self._apply_bit_transition(ch, next_value, event="random", latched_active=None)

        if self.signals > 0 and self._rng.random() < _DI_EXTERNAL_EVENT_PROB:
            ev_ch = self._rng.randrange(self.signals)
            kind = "pulse" if self._rng.random() < 0.65 else "latch"

            task = asyncio.create_task(
                self._simulate_external_event(ev_ch, kind),
                name=f"di-event:{self.unit_id}:{ev_ch}",
            )
            self._register_aux_task(task)

    async def publish_state(self, *, packet_id: Optional[int] = None) -> None:
        async with self._lock:
            payload = encode_state_all_bit(
                StateAllBit(bitmask=self._bitmask & self._mask())
            )
        await self._publish_packet(
            topic_state(self.unit_id),
            Mode.STATE_ALL_BIT,
            payload,
            packet_id=packet_id,
            retain=True,
        )

    async def handle_packet(self, topic: str, header, payload: bytes) -> None:
        mode_value = header.mode
        try:
            cmd = Cmd(mode_value)
        except ValueError:
            cmd = None

        if cmd is not None:
            await self._send_resp(
                RespStatus.UNSUPPORTED,
                RespError.NONE,
                packet_id=header.packet_id,
            )
            return

        try:
            request = Mode(mode_value)
        except ValueError:
            self._logger.debug("Skipping unknown opcode 0x%02X from %s", mode_value, topic)
            return

        if request == Mode.REQ_ALL_BIT:
            decision = await self._maybe_fail_exchange(
                header.packet_id,
                context="DI state request",
            )
            if decision:
                return
            await self._send_resp(RespStatus.OK, packet_id=header.packet_id)
            await self.publish_state(packet_id=header.packet_id)
        elif request == Mode.REQ_SINGLE_BIT:
            ch = payload[0] if payload else 0
            value = (self._bitmask >> ch) & 0x01
            decision = await self._maybe_fail_exchange(
                header.packet_id,
                context="DI state request",
            )
            if decision:
                return
            await self._send_resp(RespStatus.OK, packet_id=header.packet_id)
            await self._publish_packet(
                topic_state(self.unit_id),
                Mode.STATE_SINGLE_BIT,
                encode_state_single_bit(StateSingleBit(ch=ch, value=value)),
                packet_id=header.packet_id,
                retain=False,
            )
        elif request == Mode.REQ_DIAG_DI:
            decision = await self._maybe_fail_exchange(
                header.packet_id,
                context="DI diagnostic request",
            )
            if decision:
                return
            await self._send_resp(RespStatus.OK, packet_id=header.packet_id)
            await self._publish_di_diagnostics(packet_id=header.packet_id)
            await self._publish_latched_state(packet_id=header.packet_id)
        else:
            self._logger.debug("Unhandled DI request %s", request)

    def _mask(self) -> int:
        return (1 << max(self.signals, 1)) - 1

    def _apply_bit_transition(
        self,
        ch: int,
        value: int,
        *,
        event: str,
        latched_active: Optional[bool],
    ) -> None:
        if ch < 0 or ch >= self.signals:
            return
        mask = 1 << ch
        current = (self._bitmask >> ch) & 0x01
        if current == value:
            return
        if value:
            self._bitmask |= mask
        else:
            self._bitmask &= ~mask
        state_mask = mask if value else 0
        self._emit_bit_delta(mask, state_mask)
        self._schedule_diag_update(mask, event=event, latched_active=latched_active)

    def _emit_bit_delta(self, changed_mask: int, state_mask: int) -> None:
        mask = changed_mask & self._mask()
        if not mask:
            return
        state = state_mask & mask

        async def _send() -> None:
            payload = encode_state_changed_bit(
                StateChangedBit(changed=mask, state=state)
            )
            await self._publish_packet(
                topic_state(self.unit_id),
                Mode.STATE_CHANGED_BIT,
                payload,
                retain=False,
            )

        task = asyncio.create_task(_send(), name=f"di-delta:{self.unit_id}")
        self._register_aux_task(task)

    def _schedule_diag_update(
        self,
        mask: int,
        *,
        event: str,
        latched_active: Optional[bool],
    ) -> None:
        async def _task() -> None:
            await self._update_diag_counters(mask, event=event, latched_active=latched_active)
            if event in ("latch", "unlatch"):
                await self._publish_latched_state()
            if event in ("latch", "unlatch", "pulse", "random"):
                await self._publish_di_diagnostics()

        task = asyncio.create_task(_task(), name=f"di-diag:{self.unit_id}")
        self._register_aux_task(task)

    async def _update_diag_counters(
        self,
        mask: int,
        *,
        event: str,
        latched_active: Optional[bool],
    ) -> None:
        limited_mask = mask & self._mask()
        if not limited_mask:
            return
        async with self._diag_lock:
            self._diag_seen |= limited_mask
            if event == "latch" and latched_active:
                self._diag_stuck |= limited_mask
                self._latched_mask |= limited_mask
                self._diag_latched_delta = limited_mask
                self._diag_latched_cause = limited_mask
            elif event == "unlatch":
                self._latched_mask &= ~limited_mask
                self._diag_latched_delta = limited_mask
                self._diag_latched_cause = 0
                if self._rng.random() < 0.6:
                    self._diag_stuck &= ~limited_mask
            elif event == "pulse":
                if self._rng.random() < 0.5:
                    self._diag_lost |= limited_mask
                else:
                    self._diag_lost &= ~limited_mask
                self._diag_latched_delta = 0
            elif event == "random":
                if self._rng.random() < 0.1:
                    self._diag_stuck |= limited_mask
                elif self._rng.random() < 0.2:
                    self._diag_stuck &= ~limited_mask
                if self._rng.random() < 0.15:
                    self._diag_lost &= ~limited_mask
                self._diag_latched_delta = 0

            mask_limit = self._mask()
            self._diag_seen &= mask_limit
            self._diag_stuck &= mask_limit
            self._diag_lost &= mask_limit
            self._latched_mask &= mask_limit

    async def _diag_snapshot(self) -> tuple[int, int, int, int, int, int]:
        async with self._diag_lock:
            return (
                self._diag_seen & self._mask(),
                self._diag_stuck & self._mask(),
                self._diag_lost & self._mask(),
                self._latched_mask & self._mask(),
                self._diag_latched_delta & self._mask(),
                self._diag_latched_cause & self._mask(),
            )

    async def _publish_di_diagnostics(self, *, packet_id: Optional[int] = None) -> None:
        seen, stuck, lost, latched, changed, cause = await self._diag_snapshot()
        payload = encode_state_diag_di(
            DiagAllDi(
                seen=seen,
                stuck=stuck,
                lost=lost,
                latched=latched,
                latched_changed=changed,
                latched_cause=cause,
            )
        )
        await self._publish_packet(
            topic_state(self.unit_id),
            Mode.STATE_DIAG_DI,
            payload,
            packet_id=packet_id,
            retain=True,
        )

    async def _publish_latched_state(self, *, packet_id: Optional[int] = None) -> None:
        async with self._diag_lock:
            payload = encode_state_latched_bit(
                StateLatchedBit(
                    latched=self._latched_mask & self._mask(),
                    changed=self._diag_latched_delta & self._mask(),
                    cause=self._diag_latched_cause & self._mask(),
                )
            )
        await self._publish_packet(
            topic_state(self.unit_id),
            Mode.STATE_LATCHED_DI,
            payload,
            packet_id=packet_id,
            retain=True,
        )

    async def _simulate_external_event(self, ch: int, kind: str) -> None:
        try:
            async with self._lock:
                previous = (self._bitmask >> ch) & 0x01
            if kind == "latch":
                if previous == 1:
                    return
                self._apply_bit_transition(ch, 1, event="latch", latched_active=True)
                if self._rng.random() < 0.25:
                    await self.publish_state()
                await asyncio.sleep(self._rng.uniform(*_DI_LATCH_DURATION_RANGE))
                current = (self._bitmask >> ch) & 0x01
                if current == 1:
                    self._apply_bit_transition(
                        ch,
                        previous,
                        event="unlatch",
                        latched_active=False,
                    )
                    if self._rng.random() < 0.2:
                        await self.publish_state()
            else:
                target = 0 if previous == 1 else 1
                self._apply_bit_transition(ch, target, event="pulse", latched_active=None)
                if self._rng.random() < 0.25:
                    await self.publish_state()
                await asyncio.sleep(self._rng.uniform(*_DI_PULSE_DURATION_RANGE))
                current = (self._bitmask >> ch) & 0x01
                if current == target:
                    self._apply_bit_transition(
                        ch,
                        previous,
                        event="pulse",
                        latched_active=None,
                    )
                    if self._rng.random() < 0.2:
                        await self.publish_state()
        except asyncio.CancelledError:  # pragma: no cover - cooperative cancellation
            pass
