"""Digital output (DO) device simulator."""

from __future__ import annotations

import asyncio

from typing import TYPE_CHECKING, Awaitable, Callable, Optional

from simulator.mqtt_client import (
    SimulatedDeviceBase,
    topic_state,
    topic_state_channel,
)
from simulator.packet_structures import (
    Cmd,
    CmdSetAllBit,
    CmdSetPairBit,
    CmdSetPulseBit,
    CmdSetSingleBit,
    Mode,
    RespError,
    RespStatus,
    StateAllBit,
    StateSingleBit,
    encode_state_all_bit,
    encode_state_single_bit,
    decode_cmd_set_all_bit,
    decode_cmd_set_pair,
    decode_cmd_set_pulse,
    decode_cmd_set_single_bit,
)


if TYPE_CHECKING:  # pragma: no cover - hints only
    from simulator.mqtt_client import BehaviorSettings, BrokerSettings


COMMAND_ERROR_RATE = 0.05


class SimulatedDODevice(SimulatedDeviceBase):
    """Emulates an ESP32 board controlling digital outputs."""

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
        self._bitmask = 0
        self._state_lock = asyncio.Lock()

    @property
    def device_type_code(self) -> str:
        return "DO  "

    def randomize_state(self) -> None:
        if self.signals <= 0:
            return
        ch = self._rng.randrange(self.signals)
        value = self._rng.randint(0, 1)
        self._set_bit(ch, value)

    def should_auto_publish(self) -> bool:
        return False

    async def publish_state(self, *, packet_id: Optional[int] = None) -> None:
        async with self._state_lock:
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

    async def _publish_single_bit(
        self,
        ch: int,
        value: int,
        *,
        packet_id: Optional[int] = None,
    ) -> None:
        payload = encode_state_single_bit(StateSingleBit(ch=ch, value=value))
        await self._publish_packet(
            topic_state_channel(self.unit_id, ch),
            Mode.STATE_SINGLE_BIT,
            payload,
            packet_id=packet_id,
            retain=False,
        )

    async def handle_packet(self, topic: str, header, payload: bytes) -> None:
        mode = header.mode
        try:
            cmd = Cmd(mode)
        except ValueError:
            cmd = None

        if cmd is not None:
            await self._handle_command(cmd, payload, header.packet_id)
            return

        try:
            state_mode = Mode(mode)
        except ValueError:
            self._logger.debug("Unhandled opcode 0x%02X from %s", mode, topic)
            return

        if state_mode == Mode.REQ_ALL_BIT:
            decision = await self._maybe_fail_exchange(
                header.packet_id,
                context="DO state request",
            )
            if decision:
                return
            await self._send_resp(RespStatus.OK, packet_id=header.packet_id)
            await self.publish_state(packet_id=header.packet_id)
        elif state_mode == Mode.REQ_SINGLE_BIT:
            ch = payload[0] if payload else 0
            value = (self._bitmask >> ch) & 0x01
            decision = await self._maybe_fail_exchange(
                header.packet_id,
                context="DO state request",
            )
            if decision:
                return
            await self._send_resp(RespStatus.OK, packet_id=header.packet_id)
            await self._publish_single_bit(ch, value, packet_id=header.packet_id)
        else:
            self._logger.debug("Unhandled state request %s from %s", state_mode, topic)

    async def _handle_command(self, cmd: Cmd, payload: bytes, packet_id: int) -> None:
        action: Optional[Callable[[], Awaitable[None]]] = None
        try:
            if cmd == Cmd.SET_SINGLE_BIT:
                command = decode_cmd_set_single_bit(payload)

                async def _action() -> None:
                    await self._apply_single(command, packet_id)

                action = _action
            elif cmd == Cmd.SET_ALL_BIT:
                command = decode_cmd_set_all_bit(payload)

                async def _action() -> None:
                    await self._apply_all(command, packet_id)

                action = _action
            elif cmd == Cmd.SET_PAIR_BIT:
                command = decode_cmd_set_pair(payload)

                async def _action() -> None:
                    await self._apply_pair(command, packet_id)

                action = _action
            elif cmd == Cmd.SET_PULSE_BIT:
                command = decode_cmd_set_pulse(payload)

                async def _action() -> None:
                    await self._apply_pulse(command, packet_id)

                action = _action
            else:
                await self._send_resp(
                    RespStatus.UNSUPPORTED,
                    RespError.NONE,
                    packet_id=packet_id,
                )
                return
        except ValueError as exc:
            self._logger.warning("Failed to decode command %s: %s", cmd, exc)
            await self._send_resp(
                RespStatus.BAD_REQUEST,
                RespError.ARG_VALUE,
                packet_id=packet_id,
            )
            return

        if action is None:
            return

        if self._rng.random() < COMMAND_ERROR_RATE:
            await self._send_resp(
                RespStatus.INTERNAL_ERROR,
                RespError.HW_FAILURE,
                packet_id=packet_id,
            )
            self._logger.debug(
                "Simulated DO command error for packet 0x%04X",
                packet_id & 0xFFFF,
            )
            return

        await action()
        await self._send_resp(RespStatus.OK, packet_id=packet_id)

    async def _apply_single(self, cmd: CmdSetSingleBit, packet_id: Optional[int] = None) -> None:
        async with self._state_lock:
            self._set_bit(cmd.ch, cmd.value)
        await self._publish_single_bit(cmd.ch, cmd.value, packet_id=packet_id)

    async def _apply_all(self, cmd: CmdSetAllBit, packet_id: Optional[int] = None) -> None:
        async with self._state_lock:
            self._bitmask = cmd.bitmask & self._mask()
        await self.publish_state(packet_id=packet_id)

    async def _apply_pair(
        self,
        cmd: CmdSetPairBit,
        packet_id: Optional[int] = None,
    ) -> None:
        mapping = {
            0b00: (1, 0),  # INTERMEDIATE
            0b01: (0, 0),  # OFF
            0b10: (1, 1),  # ON
        }
        desired = mapping.get(cmd.state2b)
        if desired is None:
            return
        async with self._state_lock:
            self._set_bit(cmd.ch_a, desired[0])
            self._set_bit(cmd.ch_b, desired[1])
        await self._publish_single_bit(cmd.ch_a, desired[0], packet_id=packet_id)
        await self._publish_single_bit(cmd.ch_b, desired[1], packet_id=packet_id)

    async def _apply_pulse(
        self,
        cmd: CmdSetPulseBit,
        packet_id: Optional[int] = None,
    ) -> None:
        async with self._state_lock:
            self._set_bit(cmd.ch, cmd.value)
        await self._publish_single_bit(cmd.ch, cmd.value, packet_id=packet_id)

        async def revert() -> None:
            try:
                await asyncio.sleep(cmd.pulse_ms / 1000)
                async with self._state_lock:
                    self._set_bit(cmd.ch, 0)
                await self._publish_single_bit(cmd.ch, 0)
                await self.publish_state()
            except asyncio.CancelledError:  # pragma: no cover - cooperative cancellation
                pass

        task = asyncio.create_task(revert(), name=f"pulse:{self.unit_id}:{cmd.ch}")
        self._register_aux_task(task)

    def _mask(self) -> int:
        return (1 << max(self.signals, 1)) - 1

    def _set_bit(self, ch: int, value: int) -> None:
        if ch < 0 or ch >= self.signals:
            return
        if value:
            self._bitmask |= (1 << ch)
        else:
            self._bitmask &= ~(1 << ch)
