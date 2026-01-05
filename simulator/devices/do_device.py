"""Digital output (DO) device simulator."""

from __future__ import annotations

import asyncio

from typing import TYPE_CHECKING, Awaitable, Callable, Optional

from simulator.mqtt_client import (
    SimulatedDeviceBase,
    topic_state,
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
    StateDiagBitmask,
    StateSingleBit,
    encode_state_all_bit,
    encode_state_diag_bitmask,
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
        self._diag_lock = asyncio.Lock()
        self._diag_open = 0
        self._diag_fault = 0
        self._diag_soft = 0

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
            topic_state(self.unit_id),
            Mode.STATE_SINGLE_BIT,
            payload,
            packet_id=packet_id,
            retain=False,
        )

    async def _publish_diag(self, *, packet_id: Optional[int] = None) -> None:
        async with self._diag_lock:
            payload = encode_state_diag_bitmask(
                StateDiagBitmask(
                    open_mask=self._diag_open & self._mask(),
                    fault_mask=self._diag_fault & self._mask(),
                    soft_mask=self._diag_soft & self._mask(),
                )
            )
        await self._publish_packet(
            topic_state(self.unit_id),
            Mode.DIAG_ALL_BIT,
            payload,
            packet_id=packet_id,
            retain=True,
        )

    async def _maybe_mutate_diag(self, changed_mask: int) -> None:
        mask = changed_mask & self._mask()
        if not mask:
            return
        if self._rng.random() >= 0.35:
            return
        target_field = self._rng.choice(("_diag_open", "_diag_fault", "_diag_soft"))
        async with self._diag_lock:
            current = getattr(self, target_field)
            if self._rng.random() < 0.5:
                current |= mask
            else:
                current &= ~mask
            setattr(self, target_field, current & self._mask())
        await self._publish_diag()

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
        elif state_mode == Mode.REQ_DIAG_ALL_BIT:
            decision = await self._maybe_fail_exchange(
                header.packet_id,
                context="DO diagnostic request",
            )
            if decision:
                return
            await self._send_resp(RespStatus.OK, packet_id=header.packet_id)
            await self._publish_diag(packet_id=header.packet_id)
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
        changed_mask = 0
        async with self._state_lock:
            previous = (self._bitmask >> cmd.ch) & 0x01
            if previous != cmd.value:
                self._set_bit(cmd.ch, cmd.value)
                changed_mask = 1 << cmd.ch
        await self._publish_single_bit(cmd.ch, cmd.value, packet_id=packet_id)
        if changed_mask:
            await self._maybe_mutate_diag(changed_mask)

    async def _apply_all(self, cmd: CmdSetAllBit, packet_id: Optional[int] = None) -> None:
        changed_mask = 0
        async with self._state_lock:
            previous = self._bitmask
            self._bitmask = cmd.bitmask & self._mask()
            changed_mask = (previous ^ self._bitmask) & self._mask()
        await self.publish_state(packet_id=packet_id)
        if changed_mask:
            await self._maybe_mutate_diag(changed_mask)

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
        changed_mask = 0
        async with self._state_lock:
            prev_a = (self._bitmask >> cmd.ch_a) & 0x01
            prev_b = (self._bitmask >> cmd.ch_b) & 0x01
            self._set_bit(cmd.ch_a, desired[0])
            self._set_bit(cmd.ch_b, desired[1])
            if prev_a != desired[0]:
                changed_mask |= 1 << cmd.ch_a
            if prev_b != desired[1]:
                changed_mask |= 1 << cmd.ch_b
        await self._publish_single_bit(cmd.ch_a, desired[0], packet_id=packet_id)
        await self._publish_single_bit(cmd.ch_b, desired[1], packet_id=packet_id)
        if changed_mask:
            await self._maybe_mutate_diag(changed_mask)

    async def _apply_pulse(
        self,
        cmd: CmdSetPulseBit,
        packet_id: Optional[int] = None,
    ) -> None:
        changed_mask = 0
        async with self._state_lock:
            previous = (self._bitmask >> cmd.ch) & 0x01
            self._set_bit(cmd.ch, cmd.value)
            if previous != cmd.value:
                changed_mask = 1 << cmd.ch
        await self._publish_single_bit(cmd.ch, cmd.value, packet_id=packet_id)
        if changed_mask:
            await self._maybe_mutate_diag(changed_mask)

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
