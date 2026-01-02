# protocol/modes.py
# Opcode definitions (mirror of C++ protocol/Modes.h)

from enum import IntEnum

class State(IntEnum):
    # Digital (bit-based)
    REQ_SINGLE_BIT     = 0x10  # [ch]
    REQ_ALL_BIT        = 0x11  # []
    STATE_SINGLE_BIT   = 0x12  # [ch][val:u8]
    STATE_ALL_BIT      = 0x13  # [bitmap:u32 BE]
    REQ_ALL_DIAG       = 0x17  # [] aggregated diagnostics bitmasks
    STATE_ALL_DIAG     = 0x18  # [open:u32][fault:u32][soft:u32]
    STATE_CHANGED_BIT  = 0x19  # [changed:u32][state:u32] (DI delta snapshot)
    REQ_DIAG_DI        = 0x1A  # [] request DI diagnostics summary
    STATE_DIAG_DI      = 0x1B  # [seen][stuck][lost][latched][latched_changed][latched_cause]
    STATE_LATCHED_DI   = 0x1C  # [latched][changed][cause]

    # Analog (float-based)
    REQ_SINGLE_FLOAT   = 0x14  # [ch]
    REQ_ALL_FLOAT      = 0x15  # snapshot (for all STATE_SINGLE_FLOAT)
    STATE_SINGLE_FLOAT = 0x16  # [ch][value:floatBE]


class Cmd(IntEnum):
    # Digital Outputs
    SET_SINGLE_BIT   = 0x20  # [ch][val:u8]
    SET_ALL_BIT      = 0x21  # [bitmap:u32 BE]
    SET_PAIR_BIT     = 0x22  # [chA][chB][state2b]
    SET_PULSE_BIT    = 0x23  # [ch][val:u8][pulse_ms:u16]

    # Analog Outputs
    SET_SINGLE_FLOAT = 0x30  # [ch][value:floatBE]


class Sys(IntEnum):
    HEARTBEAT  = 0xF0  # []
    SCAN       = 0xF1  # []
    DISCONNECT = 0xF2  # []
    RESP       = 0xF3  # [status:u8][errCode:u8]
    REGISTER   = 0xF4  # [type[4]][id[32]][fwVersion:u16][num_channels:u16]
