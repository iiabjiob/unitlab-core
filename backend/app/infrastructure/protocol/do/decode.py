from ..endian import read_u32_be
from ..common.messages import DOStateSingle, DOStateAll

def dec_state_single(payload: bytes) -> DOStateSingle:
    if len(payload) != 2:
        raise ValueError(f"DO.STATE_SINGLE payload must be 2 bytes, got {len(payload)}")
    return DOStateSingle(ch=payload[0], value=payload[1])

def dec_state_all(payload: bytes) -> DOStateAll:
    if len(payload) != 4:
        raise ValueError(f"DO.STATE_ALL payload must be 4 bytes, got {len(payload)}")
    return DOStateAll(bitmap=read_u32_be(payload))
