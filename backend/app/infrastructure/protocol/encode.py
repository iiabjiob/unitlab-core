# protocol/encode.py
# Encode helpers (mirror of C++ protocol/*/encode)
import struct
from . import endian
from .packet_structures import (
    StateSingleBit,
    StateAllBit,
    CmdSetSingleBit,
    CmdSetAllBit,
    CmdSetPairBit,
    CmdSetPulseBit,
    StateSingleFloat,
    CmdSetSingleFloat,
    Resp,
    Register,
)

PairStateMask = 0x03  # from C++ Config.h (mask for 2-bit state)


# ------------------------------------------------------
# Bit (digital) encoders
# ------------------------------------------------------
class bit:
    @staticmethod
    def state_single(p: StateSingleBit) -> bytes:
        return bytes([p.ch, p.value & 0x01])

    @staticmethod
    def state_all(p: StateAllBit) -> bytes:
        return endian.write_u32_be(p.bitmask)

    @staticmethod
    def cmd_set_single(p: CmdSetSingleBit) -> bytes:
        return bytes([p.ch, p.value & 0x01])

    @staticmethod
    def cmd_set_all(p: CmdSetAllBit) -> bytes:
        return endian.write_u32_be(p.bitmask)

    @staticmethod
    def cmd_set_pair(p: CmdSetPairBit) -> bytes:
        return bytes([p.chA, p.chB, p.state2b & PairStateMask])
    
    @staticmethod
    def cmd_set_pulse(p: CmdSetPulseBit) -> bytes:
        out = bytearray(4)
        out[0] = p.ch & 0xFF
        out[1] = p.value & 0x01
        endian.write_u16_be(p.pulse_ms & 0xFFFF, into=out, offset=2)
        return bytes(out)


# ------------------------------------------------------
# Float (analog) encoders
# ------------------------------------------------------
class afloat:
    @staticmethod
    def state_single(p: StateSingleFloat) -> bytes:
        return bytes([p.ch]) + struct.pack(">f", float(p.value))

    @staticmethod
    def cmd_set_single(p: CmdSetSingleFloat) -> bytes:
        return bytes([p.ch]) + struct.pack(">f", float(p.value))


# ------------------------------------------------------
# System encoders
# ------------------------------------------------------
class sys:
    @staticmethod
    def resp(p: Resp) -> bytes:
        return bytes([p.status, p.errCode])

    @staticmethod
    def register_msg(p: Register) -> bytes:
        # type[4] + id[32] + fwVersion:u16 BE + num_channels:u16 BE
        out = bytearray(4 + 32 + 2 + 2)
        out[0:4] = p.type.encode("ascii")[:4].ljust(4, b"\x00")
        out[4:36] = p.id.encode("ascii")[:32].ljust(32, b"\x00")
        endian.write_u16_be(p.fwVersion, into=out, offset=36)
        endian.write_u16_be(p.num_channels, into=out, offset=38)
        return bytes(out)
