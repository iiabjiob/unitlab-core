# protocol/modes.py
# Opcode definitions (mirror of C++ protocol/Modes.h)

from enum import IntEnum

class State(IntEnum):
    # Digital (bit-based)
    REQ_SINGLE_BIT     = 0x10  # [ch]
    REQ_ALL_BIT        = 0x11  # []
    STATE_SINGLE_BIT   = 0x12  # [ch][val:u8]
    STATE_ALL_BIT      = 0x13  # [bitmap:u32 BE]
    STATE_CHANGED_BIT  = 0x14  # [changed:u32][state:u32] (DI delta snapshot)
    STATE_LATCHED_BIT  = 0x15  # [latched:u32][changed:u32][cause:u32]
    REQ_DIAG_DI_BIT    = 0x16  # [] request DI diagnostics summary
    DIAG_DI_BIT        = 0x17  # [seen][stuck][lost][latched][latched_changed][latched_cause]
    REQ_DIAG_ALL_BIT   = 0x18  # [] aggregated diagnostics bitmasks
    DIAG_ALL_BIT       = 0x19  # [open:u32][fault:u32][soft:u32]

    # Analog (float-based)
    REQ_SINGLE_FLOAT   = 0x1A  # [ch]
    REQ_ALL_FLOAT      = 0x1B  # snapshot (for all STATE_SINGLE_FLOAT)
    DIAG_DI_BIT_V2     = REQ_ALL_FLOAT  # firmware >=2025.2 sends DI diagnostics under this opcode
    STATE_SINGLE_FLOAT = 0x1C  # [ch][value:floatBE]
    REQ_DIAG_AO_FLOAT  = 0x1D  # [] request AO diagnostics summary
    DIAG_AO_FLOAT      = 0x1E  # [diagnostic masks]


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
    REGISTER   = 0xF4  # [type[4]][id[32]][fwVersion:u16][channels:u16]
