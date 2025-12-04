"""Analog output (AO) device simulator."""

from __future__ import annotations

import asyncio

from typing import TYPE_CHECKING, Optional

from simulator.mqtt_client import (
    SimulatedDeviceBase,
    topic_state,
)
from simulator.packet_structures import (
    Cmd,
    CmdSetSingleFloat,
    Mode,
    RespError,
    RespStatus,
    StateSingleFloat,
    encode_state_single_float,
    decode_cmd_set_single_float,
)


if TYPE_CHECKING:  # pragma: no cover - hints only
    from simulator.mqtt_client import BehaviorSettings, BrokerSettings


COMMAND_ERROR_RATE = 0.05


class SimulatedAODevice(SimulatedDeviceBase):
    """Emulates an ESP32 driving analog outputs with float precision."""

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
        self._values = [0.0 for _ in range(signals or 1)]
        self._lock = asyncio.Lock()

    @property
    def device_type_code(self) -> str:
        return "AO  "

    def randomize_state(self) -> None:
        if not self.behavior.randomize or not self._values:
            return
        idx = self._rng.randrange(len(self._values))
        delta = self._rng.uniform(-0.5, 0.5)
        self._values[idx] = float(min(max(self._values[idx] + delta, 0.0), 100.0))

    def should_auto_publish(self) -> bool:
        return False

    async def publish_state(self, *, packet_id: Optional[int] = None) -> None:
        async with self._lock:
            snapshot = list(self._values)
        for idx, value in enumerate(snapshot):
            payload = encode_state_single_float(StateSingleFloat(ch=idx, value=value))
            await self._publish_packet(
                topic_state(self.unit_id),
                Mode.STATE_SINGLE_FLOAT,
                payload,
                packet_id=packet_id,
                retain=True,
            )
            packet_id = None  # reuse only once per burst

    async def handle_packet(self, topic: str, header, payload: bytes) -> None:
        mode_value = header.mode
        try:
            cmd = Cmd(mode_value)
        except ValueError:
            cmd = None

        if cmd is not None:
            await self._handle_command(cmd, payload, header.packet_id)
            return

        try:
            request = Mode(mode_value)
        except ValueError:
            self._logger.debug("Ignoring unknown opcode 0x%02X from %s", mode_value, topic)
            return

        if request == Mode.REQ_ALL_FLOAT:
            decision = await self._maybe_fail_exchange(
                header.packet_id,
                context="AO state request",
            )
            if decision:
                return
            await self._send_resp(RespStatus.OK, packet_id=header.packet_id)
            await self.publish_state(packet_id=header.packet_id)
        elif request == Mode.REQ_SINGLE_FLOAT:
            ch = payload[0] if payload else 0
            value = self._value_at(ch)
            decision = await self._maybe_fail_exchange(
                header.packet_id,
                context="AO state request",
            )
            if decision:
                return
            await self._send_resp(RespStatus.OK, packet_id=header.packet_id)
            await self._publish_value(ch, value, packet_id=header.packet_id)
        else:
            self._logger.debug("Unhandled AO request %s", request)

    async def _handle_command(self, cmd: Cmd, payload: bytes, packet_id: int) -> None:
        if cmd != Cmd.SET_SINGLE_FLOAT:
            await self._send_resp(
                RespStatus.UNSUPPORTED,
                RespError.NONE,
                packet_id=packet_id,
            )
            return
        try:
            command = decode_cmd_set_single_float(payload)
        except ValueError as exc:
            self._logger.warning("Invalid AO command: %s", exc)
            await self._send_resp(
                RespStatus.BAD_REQUEST,
                RespError.ARG_VALUE,
                packet_id=packet_id,
            )
            return

        if self._rng.random() < COMMAND_ERROR_RATE:
            await self._send_resp(
                RespStatus.INTERNAL_ERROR,
                RespError.HW_FAILURE,
                packet_id=packet_id,
            )
            self._logger.debug(
                "Simulated AO command error for packet 0x%04X",
                packet_id & 0xFFFF,
            )
            return

        async with self._lock:
            if command.ch >= len(self._values):
                await self._send_resp(
                    RespStatus.BAD_REQUEST,
                    RespError.ARG_RANGE,
                    packet_id=packet_id,
                )
                return
            self._values[command.ch] = command.value
        await self._send_resp(RespStatus.OK, packet_id=packet_id)
        await self._publish_value(command.ch, command.value, packet_id=packet_id)

    async def _publish_value(
        self,
        ch: int,
        value: float,
        *,
        packet_id: Optional[int] = None,
    ) -> None:
        payload = encode_state_single_float(StateSingleFloat(ch=ch, value=value))
        await self._publish_packet(
            topic_state(self.unit_id),
            Mode.STATE_SINGLE_FLOAT,
            payload,
            packet_id=packet_id,
            retain=True,
        )

    def _value_at(self, ch: int) -> float:
        if 0 <= ch < len(self._values):
            return self._values[ch]
        return 0.0
