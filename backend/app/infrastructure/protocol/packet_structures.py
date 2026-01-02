# protocol/packet_structures.py
# Mirror of C++ protocol/PacketStructures.h
from pydantic import BaseModel
from enum import IntEnum

# ------------------------------------------------------------------------
# Digital (DI/DO) state
# ------------------------------------------------------------------------


class ReqStateSingleBit(BaseModel):
    ch: int  # u8


# ReqStateAllBit → empty payload



class StateSingleBit(BaseModel):
    ch: int   # u8
    value: int  # u8 (0=off, 1=on)



class StateAllBit(BaseModel):
    bitmask: int  # u32 (big-endian)



class StateDiagBitmask(BaseModel):
    open_mask: int   # u32
    fault_mask: int  # u32
    soft_mask: int   # u32



class StateChangedBit(BaseModel):
    changed: int  # u32 bitmask
    state: int    # u32 snapshot (only bits from 'changed' are valid)


class DiagAllDi(BaseModel):
    seen: int
    stuck: int
    lost: int
    latched: int
    latched_changed: int
    latched_cause: int


class StateLatchedBit(BaseModel):
    latched: int
    changed: int
    cause: int



class CmdSetSingleBit(BaseModel):
    ch: int
    value: int  # u8 (0=off, 1=on)



class CmdSetAllBit(BaseModel):
    bitmask: int  # u32 (big-endian)



class CmdSetPairBit(BaseModel):
    chA: int
    chB: int
    state2b: int  # lowest 2 bits


class CmdSetPulseBit(BaseModel):
    ch: int
    value: int
    pulse_ms: int  # u16 (big-endian)


class PairState2b(IntEnum):
    INTERMEDIATE = 0b00
    OFF          = 0b01
    ON           = 0b10
    INVALID      = 0b11


PairStateMask: int = 0b11


# ------------------------------------------------------------------------
# Analog (AO) state
# ------------------------------------------------------------------------


class ReqStateSingleFloat(BaseModel):
    ch: int


# ReqStateAllFloat → empty payload



class StateSingleFloat(BaseModel):
    ch: int
    value: float  # big-endian encoded float



class CmdSetSingleFloat(BaseModel):
    ch: int
    value: float  # big-endian encoded float


# ------------------------------------------------------------------------
# System (SYS)
# ------------------------------------------------------------------------


class Resp(BaseModel):
    status: int  # u8
    errCode: int  # u8



class Register(BaseModel):
    type: str     # 4-char ASCII string
    id: str       # up to 32-char ASCII string
    fwVersion: int  # u16
    num_channels: int   # u16

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
    HW_FAILURE     = 0x07