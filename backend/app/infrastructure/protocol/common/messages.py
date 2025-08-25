

from dataclasses import dataclass

# -------- DO (Digital Outputs) --------
@dataclass(frozen=True)
class DOReqStateSingle:
    ch: int

@dataclass(frozen=True)
class DOStateSingle:
    ch: int
    value: int  # 0 or 1

@dataclass(frozen=True)
class DOStateAll:
    bitmap: int  # u32

@dataclass(frozen=True)
class DOCmdSetSingle:
    ch: int
    value: int  # 0 or 1

@dataclass(frozen=True)
class DOCmdSetAll:
    bitmap: int  # u32

@dataclass(frozen=True)
class DOCmdSetPair:
    ch_a: int
    ch_b: int
    state2b: int  # 2 LSBs used

# -------- DI (Digital Inputs) --------
@dataclass(frozen=True)
class DIReqStateSingle:
    ch: int

@dataclass(frozen=True)
class DIStateSingle:
    ch: int
    value: int

@dataclass(frozen=True)
class DIStateAll:
    bitmap: int

# -------- AO (Analog Outputs) --------
@dataclass(frozen=True)
class AOReqStateSingle:
    ch: int

@dataclass(frozen=True)
class AOStateSingle:
    ch: int
    value_ma: float

@dataclass(frozen=True)
class AOStateAll:
    values_ma: list[float]

@dataclass(frozen=True)
class AOCmdSetSingle:
    ch: int
    value_ma: float

@dataclass(frozen=True)
class AOCmdSetAll:
    values_ma: list[float]

# -------- SYS --------
@dataclass(frozen=True)
class SysResp:
    status: int
    err_code: int
