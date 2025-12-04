"""Digital input (DI) device simulator."""

from __future__ import annotations

import asyncio

from typing import TYPE_CHECKING, Optional

from simulator.mqtt_client import (
    SimulatedDeviceBase,
    topic_state,
    topic_state_channel,
)
from simulator.packet_structures import (
    Cmd,
    Mode,
    RespError,
    RespStatus,
    StateAllBit,
    StateSingleBit,
    encode_state_all_bit,
    encode_state_single_bit,
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

    @property
    def device_type_code(self) -> str:
        return "DI  "

    def randomize_state(self) -> None:
        if self.signals <= 0:
            return
        ch = self._rng.randrange(self.signals)
        if (self._bitmask >> ch) & 0x01:
            self._bitmask &= ~(1 << ch)
        else:
            self._bitmask |= 1 << ch

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
                topic_state_channel(self.unit_id, ch),
                Mode.STATE_SINGLE_BIT,
                encode_state_single_bit(StateSingleBit(ch=ch, value=value)),
                packet_id=header.packet_id,
                retain=False,
            )
        else:
            self._logger.debug("Unhandled DI request %s", request)

    def _mask(self) -> int:
        return (1 << max(self.signals, 1)) - 1

    async def _simulate_external_event(self, ch: int, kind: str) -> None:
        try:
            async with self._lock:
                previous = (self._bitmask >> ch) & 0x01
                if kind == "latch":
                    if previous == 1:
                        return  # already latched, skip duplicate
                    target = 1
                else:  # pulse toggles
                    target = 0 if previous == 1 else 1
                if target:
                    self._bitmask |= (1 << ch)
                else:
                    self._bitmask &= ~(1 << ch)
                payload = encode_state_single_bit(StateSingleBit(ch=ch, value=target))
            await self._publish_packet(
                topic_state_channel(self.unit_id, ch),
                Mode.STATE_SINGLE_BIT,
                payload,
                retain=False,
            )
            if self._rng.random() < 0.25:
                await self.publish_state()

            if kind == "pulse":
                await asyncio.sleep(self._rng.uniform(*_DI_PULSE_DURATION_RANGE))
                async with self._lock:
                    final = previous
                    if final:
                        self._bitmask |= (1 << ch)
                    else:
                        self._bitmask &= ~(1 << ch)
                    payload = encode_state_single_bit(StateSingleBit(ch=ch, value=final))
                await self._publish_packet(
                    topic_state_channel(self.unit_id, ch),
                    Mode.STATE_SINGLE_BIT,
                    payload,
                    retain=False,
                )
                if self._rng.random() < 0.2:
                    await self.publish_state()
            else:  # latch
                await asyncio.sleep(self._rng.uniform(*_DI_LATCH_DURATION_RANGE))
                if previous != target:
                    async with self._lock:
                        if previous:
                            self._bitmask |= (1 << ch)
                        else:
                            self._bitmask &= ~(1 << ch)
                        payload = encode_state_single_bit(
                            StateSingleBit(ch=ch, value=previous)
                        )
                    await self._publish_packet(
                        topic_state_channel(self.unit_id, ch),
                        Mode.STATE_SINGLE_BIT,
                        payload,
                    )
                    if self._rng.random() < 0.2:
                        await self.publish_state()
        except asyncio.CancelledError:  # pragma: no cover - cooperative cancellation
            pass
