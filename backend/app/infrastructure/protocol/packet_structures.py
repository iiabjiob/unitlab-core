# protocol/packet_structures.py
# Mirror of C++ protocol/PacketStructures.h
from dataclasses import dataclass
from enum import IntEnum
from typing import Optional


# ------------------------------------------------------------------------
# Digital (DI/DO) state
# ------------------------------------------------------------------------

@dataclass
class ReqStateSingleBit:
    ch: int  # u8


# ReqStateAllBit → empty payload


@dataclass
class StateSingleBit:
    ch: int   # u8
    value: int  # u8 (0=off, 1=on)


@dataclass
class StateAllBit:
    bitmask: int  # u32 (big-endian)


@dataclass
class CmdSetSingleBit:
    ch: int
    value: int  # u8 (0=off, 1=on)


@dataclass
class CmdSetAllBit:
    bitmask: int  # u32 (big-endian)


@dataclass
class CmdSetPairBit:
    chA: int
    chB: int
    state2b: int  # lowest 2 bits


class PairState2b(IntEnum):
    INTERMEDIATE = 0b00
    OFF          = 0b01
    ON           = 0b10
    INVALID      = 0b11


PairStateMask: int = 0b11


# ------------------------------------------------------------------------
# Analog (AO) state
# ------------------------------------------------------------------------

@dataclass
class ReqStateSingleFloat:
    ch: int


# ReqStateAllFloat → empty payload


@dataclass
class StateSingleFloat:
    ch: int
    value: float  # big-endian encoded float


@dataclass
class CmdSetSingleFloat:
    ch: int
    value: float  # big-endian encoded float


# ------------------------------------------------------------------------
# System (SYS)
# ------------------------------------------------------------------------

@dataclass
class Resp:
    status: int  # u8
    errCode: int  # u8


@dataclass
class Register:
    type: str     # 4-char ASCII string
    id: str       # up to 32-char ASCII string
    fwVersion: int  # u16
    channels: int   # u16


# ------------------------------------------------------------------------
# System enums (high-level status & errors)
# ------------------------------------------------------------------------

class RespStatus(IntEnum):
    OK             = 0x00
    BAD_REQUEST    = 0x01
    UNSUPPORTED    = 0x02
    BUSY           = 0x03
    TIMEOUT        = 0x04
    INTERNAL_ERROR = 0x05
    INVALID_STATE  = 0x06


class RespError(IntEnum):
    NONE           = 0x00
    ARG_RANGE      = 0x01
    ARG_VALUE      = 0x02
    NOT_READY      = 0x03
    STORAGE_FAIL   = 0x04
    TRANSPORT_FAIL = 0x05
    PERMISSION     = 0x06
    # 0x07..0xFF reserved
