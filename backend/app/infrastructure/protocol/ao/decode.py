
from ..endian import read_f32_be
from ..common.messages import AOStateSingle, AOStateAll

def dec_state_single(payload: bytes) -> AOStateSingle:
    if len(payload) != 5:
        raise ValueError(f"AO.STATE_SINGLE payload must be 5 bytes, got {len(payload)}")
    ch = payload[0]
    value_ma = read_f32_be(payload[1:5])
    return AOStateSingle(ch=ch, value_ma=float(value_ma))

def dec_state_all(payload: bytes) -> AOStateAll:
    if not payload:
        raise ValueError("AO.STATE_ALL payload must not be empty")
    count = payload[0]
    expected = 1 + 4*count
    if len(payload) != expected:
        raise ValueError(f"AO.STATE_ALL bad length: got {len(payload)}, expected {expected}")
    values = []
    for i in range(count):
        start = 1 + 4*i
        values.append(float(read_f32_be(payload[start:start+4])))
    return AOStateAll(values_ma=values)
