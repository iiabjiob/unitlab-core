
from ..endian import read_u32_be
from ..common.messages import DIStateSingle, DIStateAll

def dec_state_single(payload: bytes) -> DIStateSingle:
    if len(payload) != 2:
        raise ValueError(f"DI.STATE_SINGLE payload must be 2 bytes, got {len(payload)}")
    return DIStateSingle(ch=payload[0], value=payload[1])

def dec_state_all(payload: bytes) -> DIStateAll:
    if len(payload) != 4:
        raise ValueError(f"DI.STATE_ALL payload must be 4 bytes, got {len(payload)}")
    return DIStateAll(bitmap=read_u32_be(payload))
