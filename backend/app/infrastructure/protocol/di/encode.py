
from ..common.messages import DIReqStateSingle

def enc_req_state_single(msg: DIReqStateSingle) -> bytes:
    return bytes([msg.ch & 0xFF])

def enc_req_state_all() -> bytes:
    return b""
