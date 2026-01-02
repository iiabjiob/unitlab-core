# protocol/decode.py
# Decode helpers (mirror of C++ protocol/*/decode)

import struct
from typing import Optional
from . import endian
from .packet_structures import (
    StateSingleBit,
    StateAllBit,
    StateDiagBitmask,
    StateChangedBit,
    DiagAllDi,
    StateLatchedBit,
    CmdSetSingleBit,
    CmdSetAllBit,
    CmdSetPairBit,
    CmdSetPulseBit,
    StateSingleFloat,
    CmdSetSingleFloat,
    Resp,
    Register,
    PairStateMask,
)


# ------------------------------------------------------
# Bit (digital) decoders
# ------------------------------------------------------
class bit:
    @staticmethod
    def state_single(data: bytes) -> Optional[StateSingleBit]:
        if len(data) != 2:
            return None
        return StateSingleBit(
            ch=data[0], 
            value=data[1]
        )

    @staticmethod
    def state_all(data: bytes) -> Optional[StateAllBit]:
        if len(data) != 4:
            return None
        return StateAllBit(bitmask=endian.read_u32_be(data))

    @staticmethod
    def state_diag(data: bytes) -> Optional[StateDiagBitmask]:
        if len(data) != 12:
            return None
        return StateDiagBitmask(
            open_mask=endian.read_u32_be(data, 0),
            fault_mask=endian.read_u32_be(data, 4),
            soft_mask=endian.read_u32_be(data, 8),
        )

    @staticmethod
    def state_delta(data: bytes) -> Optional[StateChangedBit]:
        if len(data) != 8:
            return None
        return StateChangedBit(
            changed=endian.read_u32_be(data, 0),
            state=endian.read_u32_be(data, 4),
        )

    @staticmethod
    def state_diag_di(data: bytes) -> Optional[DiagAllDi]:
        if len(data) != 24:
            return None
        return DiagAllDi(
            seen=endian.read_u32_be(data, 0),
            stuck=endian.read_u32_be(data, 4),
            lost=endian.read_u32_be(data, 8),
            latched=endian.read_u32_be(data, 12),
            latched_changed=endian.read_u32_be(data, 16),
            latched_cause=endian.read_u32_be(data, 20),
        )

    @staticmethod
    def state_latched(data: bytes) -> Optional[StateLatchedBit]:
        if len(data) != 12:
            return None
        return StateLatchedBit(
            latched=endian.read_u32_be(data, 0),
            changed=endian.read_u32_be(data, 4),
            cause=endian.read_u32_be(data, 8),
        )

    @staticmethod
    def cmd_set_single(data: bytes) -> Optional[CmdSetSingleBit]:
        if len(data) != 2:
            return None
        return CmdSetSingleBit(
            ch=data[0], 
            value=data[1]
        )

    @staticmethod
    def cmd_set_all(data: bytes) -> Optional[CmdSetAllBit]:
        if len(data) != 4:
            return None
        return CmdSetAllBit(bitmask=endian.read_u32_be(data))

    @staticmethod
    def cmd_set_pair(data: bytes) -> Optional[CmdSetPairBit]:
        if len(data) != 3:
            return None
        return CmdSetPairBit(
            chA=data[0], 
            chB=data[1], 
            state2b=data[2] & PairStateMask
        )

    @staticmethod
    def cmd_set_pulse(data: bytes) -> Optional[CmdSetPulseBit]:
        if len(data) != 4:
            return None

        return CmdSetPulseBit(
            ch=data[0],
            value=data[1] & 0x01,
            pulse_ms=endian.read_u16_be(data, 2),
        )


# ------------------------------------------------------
# Float (analog) decoders
# ------------------------------------------------------
class afloat:
    @staticmethod
    def state_single(data: bytes) -> Optional[StateSingleFloat]:
        if len(data) != 5:
            return None
        value = struct.unpack(">f", data[1:5])[0]
        return StateSingleFloat(ch=data[0], value=value)

    @staticmethod
    def cmd_set_single(data: bytes) -> Optional[CmdSetSingleFloat]:
        if len(data) != 5:
            return None
        value = struct.unpack(">f", data[1:5])[0]
        return CmdSetSingleFloat(ch=data[0], value=value)


# ------------------------------------------------------
# System decoders
# ------------------------------------------------------
class sys:
    @staticmethod
    def resp(data: bytes) -> Optional[Resp]:
        if len(data) != 2:
            return None
        return Resp(status=data[0], errCode=data[1])

    @staticmethod
    def register_msg(data: bytes) -> Optional[Register]:
        if len(data) != 40:
            return None
        type_str = data[0:4].rstrip(b"\x00").decode("ascii", errors="ignore")
        id_str   = data[4:36].rstrip(b"\x00").decode("ascii", errors="ignore")

        fw_num   = endian.read_u16_be(data, 36)   # u16 raw
        num_channels = endian.read_u16_be(data, 38)

        return Register(
            type=type_str,
            id=id_str,
            fwVersion=fw_num,   # keep as int (u16)
            num_channels=num_channels,
        )
