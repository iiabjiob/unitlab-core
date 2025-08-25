
from ..endian import write_f32_be
from ..common.messages import AOReqStateSingle, AOCmdSetSingle, AOCmdSetAll

def enc_req_state_single(msg: AOReqStateSingle) -> bytes:
    return bytes([msg.ch & 0xFF])

def enc_req_state_all() -> bytes:
    return b""

def enc_cmd_set_single(msg: AOCmdSetSingle) -> bytes:
    return bytes([msg.ch & 0xFF]) + write_f32_be(float(msg.value_ma))

def enc_cmd_set_all(msg: AOCmdSetAll) -> bytes:
    out = bytes([len(msg.values_ma) & 0xFF])
    for v in msg.values_ma:
        out += write_f32_be(float(v))
    return out
