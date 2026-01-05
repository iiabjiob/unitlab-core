"""Binary packet helpers mirroring UnitLab's device protocol.

The simulator only implements the subset required to emulate the firmware:
header assembly/parsing plus a handful of state and command payloads. The
layout matches the backend definition found under
``app.infrastructure.protocol`` so the core service cannot distinguish a real
ESP32 from the simulator.
"""

from __future__ import annotations

import struct
import time
from dataclasses import dataclass
from enum import IntEnum
from typing import Optional, Tuple

HEADER_STRUCT = struct.Struct(">BBHQBH")  # mode, version, packet_id, ts, flags, payload_len
HEADER_SIZE = HEADER_STRUCT.size
MAX_PAYLOAD = 1024
PROTOCOL_VERSION = 1


@dataclass
class PacketHeader:
    """In-memory representation of the fixed UnitLab protocol header."""

    mode: int
    version: int
    packet_id: int
    timestamp_ms: int
    flags: int
    payload_len: int


def pack_header(header: PacketHeader) -> bytes:
    """Pack a header into bytes using the canonical big-endian layout."""
    return HEADER_STRUCT.pack(
        header.mode & 0xFF,
        header.version & 0xFF,
        header.packet_id & 0xFFFF,
        header.timestamp_ms & 0xFFFFFFFFFFFFFFFF,
        header.flags & 0xFF,
        header.payload_len & 0xFFFF,
    )


def unpack_header(data: bytes) -> PacketHeader:
    """Unpack bytes into a ``PacketHeader`` with bounds validation."""
    if len(data) < HEADER_SIZE:
        raise ValueError("Buffer too small for header")
    mode, version, packet_id, timestamp_ms, flags, payload_len = HEADER_STRUCT.unpack(
        data[:HEADER_SIZE]
    )
    if payload_len > MAX_PAYLOAD:
        raise ValueError("Payload length exceeds protocol limit")
    return PacketHeader(mode, version, packet_id, timestamp_ms, flags, payload_len)


class Mode(IntEnum):
    """State opcodes used by both the firmware and simulator."""

    # Digital state (matching backend/app/infrastructure/protocol/modes.py)
    REQ_SINGLE_BIT     = 0x10
    REQ_ALL_BIT        = 0x11
    STATE_SINGLE_BIT   = 0x12
    STATE_ALL_BIT      = 0x13
    STATE_CHANGED_BIT  = 0x14
    STATE_LATCHED_BIT  = 0x15
    REQ_DIAG_DI_BIT    = 0x16
    DIAG_DI_BIT        = 0x17
    REQ_DIAG_ALL_BIT   = 0x18
    DIAG_ALL_BIT       = 0x19

    # Analog state (AO)
    REQ_SINGLE_FLOAT   = 0x1A
    REQ_ALL_FLOAT      = 0x1B
    STATE_SINGLE_FLOAT = 0x1C
    REQ_DIAG_AO_FLOAT  = 0x1D
    DIAG_AO_FLOAT      = 0x1E


class Cmd(IntEnum):
    """Command opcodes understood by DO/AO simulators."""

    SET_SINGLE_BIT = 0x20
    SET_ALL_BIT = 0x21
    SET_PAIR_BIT = 0x22
    SET_PULSE_BIT = 0x23
    SET_SINGLE_FLOAT = 0x30


class Sys(IntEnum):
    """High-level system opcodes."""

    HEARTBEAT = 0xF0
    SCAN = 0xF1
    DISCONNECT = 0xF2
    RESP = 0xF3
    REGISTER = 0xF4


class RespStatus(IntEnum):
    OK = 0x00
    BAD_REQUEST = 0x01
    UNSUPPORTED = 0x02
    BUSY = 0x03
    TIMEOUT = 0x04
    INTERNAL_ERROR = 0x05
    INVALID_STATE = 0x06


class RespError(IntEnum):
    NONE = 0x00
    ARG_RANGE = 0x01
    ARG_VALUE = 0x02
    NOT_READY = 0x03
    STORAGE_FAIL = 0x04
    TRANSPORT_FAIL = 0x05
    PERMISSION = 0x06
    HW_FAILURE = 0x07


@dataclass
class StateSingleBit:
    ch: int
    value: int


@dataclass
class StateAllBit:
    bitmask: int


@dataclass
class StateDiagBitmask:
    open_mask: int
    fault_mask: int
    soft_mask: int


@dataclass
class StateChangedBit:
    changed: int
    state: int


@dataclass
class DiagAllDi:
    seen: int
    stuck: int
    lost: int
    latched: int
    latched_changed: int
    latched_cause: int


@dataclass
class StateLatchedBit:
    latched: int
    changed: int
    cause: int


@dataclass
class StateSingleFloat:
    ch: int
    value: float


@dataclass
class CmdSetSingleBit:
    ch: int
    value: int


@dataclass
class CmdSetAllBit:
    bitmask: int


@dataclass
class CmdSetPairBit:
    ch_a: int
    ch_b: int
    state2b: int


@dataclass
class CmdSetPulseBit:
    ch: int
    value: int
    pulse_ms: int


@dataclass
class CmdSetSingleFloat:
    ch: int
    value: float


@dataclass
class RespFrame:
    status: RespStatus
    err_code: RespError


@dataclass
class RegisterFrame:
    type_code: str
    unit_id: str
    fw_version: int
    num_channels: int


def encode_state_single_bit(payload: StateSingleBit) -> bytes:
    return bytes([payload.ch & 0xFF, payload.value & 0x01])


def encode_state_all_bit(payload: StateAllBit) -> bytes:
    return payload.bitmask.to_bytes(4, byteorder="big", signed=False)


def encode_state_diag_bitmask(payload: StateDiagBitmask) -> bytes:
    return (
        payload.open_mask.to_bytes(4, "big", signed=False)
        + payload.fault_mask.to_bytes(4, "big", signed=False)
        + payload.soft_mask.to_bytes(4, "big", signed=False)
    )


def encode_state_changed_bit(payload: StateChangedBit) -> bytes:
    return payload.changed.to_bytes(4, "big", signed=False) + payload.state.to_bytes(4, "big", signed=False)


def encode_state_diag_di(payload: DiagAllDi) -> bytes:
    return b"".join(
        value.to_bytes(4, "big", signed=False)
        for value in (
            payload.seen,
            payload.stuck,
            payload.lost,
            payload.latched,
            payload.latched_changed,
            payload.latched_cause,
        )
    )


def encode_state_latched_bit(payload: StateLatchedBit) -> bytes:
    return b"".join(
        value.to_bytes(4, "big", signed=False)
        for value in (payload.latched, payload.changed, payload.cause)
    )


def encode_state_single_float(payload: StateSingleFloat) -> bytes:
    return bytes([payload.ch & 0xFF]) + struct.pack(">f", float(payload.value))


def encode_resp(payload: RespFrame) -> bytes:
    return bytes([payload.status.value & 0xFF, payload.err_code.value & 0xFF])


def encode_register(payload: RegisterFrame) -> bytes:
    device_type = payload.type_code.encode("ascii", errors="ignore")[:4].ljust(4, b"\x00")
    unit_id = payload.unit_id.encode("ascii", errors="ignore")[:32].ljust(32, b"\x00")
    fw_version = payload.fw_version.to_bytes(2, byteorder="big", signed=False)
    num_channels = payload.num_channels.to_bytes(2, byteorder="big", signed=False)
    return device_type + unit_id + fw_version + num_channels


def decode_cmd_set_single_bit(data: bytes) -> CmdSetSingleBit:
    if len(data) != 2:
        raise ValueError("SET_SINGLE_BIT payload must be 2 bytes")
    return CmdSetSingleBit(ch=data[0], value=data[1] & 0x01)


def decode_cmd_set_all_bit(data: bytes) -> CmdSetAllBit:
    if len(data) != 4:
        raise ValueError("SET_ALL_BIT payload must be 4 bytes")
    return CmdSetAllBit(bitmask=int.from_bytes(data, "big"))


def decode_cmd_set_pair(data: bytes) -> CmdSetPairBit:
    if len(data) != 3:
        raise ValueError("SET_PAIR_BIT payload must be 3 bytes")
    return CmdSetPairBit(ch_a=data[0], ch_b=data[1], state2b=data[2] & 0x03)


def decode_cmd_set_pulse(data: bytes) -> CmdSetPulseBit:
    if len(data) != 4:
        raise ValueError("SET_PULSE_BIT payload must be 4 bytes")
    pulse_ms = int.from_bytes(data[2:4], "big")
    return CmdSetPulseBit(ch=data[0], value=data[1] & 0x01, pulse_ms=pulse_ms)


def decode_cmd_set_single_float(data: bytes) -> CmdSetSingleFloat:
    if len(data) != 5:
        raise ValueError("SET_SINGLE_FLOAT payload must be 5 bytes")
    value = struct.unpack(">f", data[1:5])[0]
    return CmdSetSingleFloat(ch=data[0], value=value)


class PacketBuilder:
    """Incremental helper used by the simulator to send packets."""

    def __init__(self) -> None:
        self._packet_id = 0

    def next_packet_id(self) -> int:
        self._packet_id = (self._packet_id + 1) & 0xFFFF
        if self._packet_id == 0:
            self._packet_id = 1
        return self._packet_id

    def build(
        self,
        mode: int,
        payload: bytes,
        *,
        timestamp_ms: Optional[int] = None,
        packet_id: Optional[int] = None,
        flags: int = 0,
        version: int = PROTOCOL_VERSION,
    ) -> bytes:
        if len(payload) > MAX_PAYLOAD:
            raise ValueError("Payload exceeds protocol maximum")
        ts = int(timestamp_ms if timestamp_ms is not None else time.time() * 1000)
        pid = packet_id if packet_id is not None else self.next_packet_id()
        header = PacketHeader(mode, version, pid, ts, flags, len(payload))
        return pack_header(header) + payload


class PacketParser:
    """Lenient parser mirroring the backend implementation."""

    def __init__(self, raw: bytes):
        if len(raw) < HEADER_SIZE:
            raise ValueError("Buffer too small for packet")
        self._raw = raw
        self._header = unpack_header(raw[:HEADER_SIZE])
        end = HEADER_SIZE + self._header.payload_len
        if len(raw) < end:
            raise ValueError("Buffer shorter than declared payload length")
        self._payload = raw[HEADER_SIZE:end]

    @property
    def header(self) -> PacketHeader:
        return self._header

    @property
    def payload(self) -> bytes:
        return self._payload


def build_packet(
    mode: IntEnum | int,
    payload: bytes,
    *,
    builder: PacketBuilder,
    timestamp_ms: Optional[int] = None,
    flags: int = 0,
    packet_id: Optional[int] = None,
) -> bytes:
    """Convenience wrapper that reuses a ``PacketBuilder`` per device."""
    mode_value = int(mode)
    return builder.build(
        mode_value,
        payload,
        timestamp_ms=timestamp_ms,
        flags=flags,
        packet_id=packet_id,
    )


def parse_packet(raw: bytes) -> Tuple[PacketHeader, bytes]:
    """Parse a packet returning the header/payload tuple."""
    parser = PacketParser(raw)
    return parser.header, parser.payload
