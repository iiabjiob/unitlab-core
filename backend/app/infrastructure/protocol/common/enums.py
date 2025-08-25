

from enum import IntEnum

class DI(IntEnum):
    REQ_STATE_SINGLE = 0x10
    REQ_STATE_ALL    = 0x11
    STATE_SINGLE     = 0x12
    STATE_ALL        = 0x13

class DO(IntEnum):
    REQ_STATE_SINGLE = 0x20
    REQ_STATE_ALL    = 0x21
    STATE_SINGLE     = 0x22
    STATE_ALL        = 0x23
    CMD_SET_SINGLE   = 0x28
    CMD_SET_ALL      = 0x29
    CMD_SET_PAIR     = 0x2C

class AO(IntEnum):
    REQ_STATE_SINGLE = 0x30
    REQ_STATE_ALL    = 0x31
    STATE_SINGLE     = 0x32
    STATE_ALL        = 0x33
    CMD_SET_SINGLE   = 0x38
    CMD_SET_ALL      = 0x39

class SYS(IntEnum):
    HEARTBEAT  = 0xF0
    SCAN       = 0xF1
    DISCONNECT = 0xF2
    RESP       = 0xF3

# Optional RESP status/enums if you define them later:
class RespStatus(IntEnum):
    OK = 0
    FAIL = 1
    # Add others according to your C++ enum later

class RespError(IntEnum):
    NONE = 0
    UNKNOWN = 1
    # Add others according to your C++ enum later
