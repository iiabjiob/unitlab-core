"""Golden wire vectors audited against the firmware protocol v1 decoder.

Channels on the wire are zero-based logical channels. Physical NCV7240
output mapping belongs to firmware and must not be applied by the backend.
"""

import struct

import pytest

from app.infrastructure.protocol.modes import Cmd
from app.core.mqtt_dto import OutboundCmdMsg
from app.services import command_queue_service as commands


@pytest.mark.anyio
@pytest.mark.parametrize(
    "mode,kwargs,payload",
    [
        *[
            (Cmd.SET_SINGLE_BIT, {"ch": ch, "value": value}, bytes([ch, value]))
            for ch in range(32)
            for value in (0, 1)
        ],
        (Cmd.SET_ALL_BIT, {"bitmask": 0x80000010}, b"\x80\x00\x00\x10"),
        *[
            (Cmd.SET_PAIR_BIT, {"chA": 4, "chB": 31, "state2b": state}, bytes([4, 31, state]))
            for state in range(4)
        ],
        (Cmd.SET_PULSE_BIT, {"ch": 4, "value": 1, "pulse_ms": 500}, b"\x04\x01\x01\xf4"),
        (Cmd.SET_PULSE_BIT, {"ch": 31, "value": 0, "pulse_ms": 65535}, b"\x1f\x00\xff\xff"),
    ],
)
async def test_do_queue_preserves_logical_channel_and_wire_layout(
    monkeypatch: pytest.MonkeyPatch,
    mode: Cmd,
    kwargs: dict[str, int],
    payload: bytes,
) -> None:
    messages: list[OutboundCmdMsg] = []

    async def reserve(unit_id: str, command_id: str) -> tuple[int, bool]:
        assert (unit_id, command_id) == ("DO-001", "audit-command")
        return 0x1234, True

    async def capture(message: OutboundCmdMsg) -> None:
        messages.append(message)

    monkeypatch.setattr(commands, "_allocate_command_packet", reserve)
    monkeypatch.setattr(commands, "enqueue_outbound_command", capture)
    result = await commands.enqueue_do_command(
        unit_id="DO-001", mode=mode, command_id="audit-command", **kwargs  # pyright: ignore[reportArgumentType]
    )

    assert result == "audit-command"
    assert len(messages) == 1
    message = messages[0]
    # Independent layout oracle: mode, version, id, timestamp, flags, length.
    opcode = {
        Cmd.SET_SINGLE_BIT: 0x20,
        Cmd.SET_ALL_BIT: 0x21,
        Cmd.SET_PAIR_BIT: 0x22,
        Cmd.SET_PULSE_BIT: 0x23,
    }[mode]
    assert message.payload == struct.pack(
        ">BBHQBH", opcode, 1, 0x1234, 0, 0, len(payload)
    ) + payload
    assert message.topic == "DO-001/c"
    assert message.packet_id == 0x1234
    assert message.command_id == result
    assert message.retain is False
