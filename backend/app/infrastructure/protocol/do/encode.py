
from ..endian import write_u32_be
from ..common.messages import DOReqStateSingle, DOCmdSetSingle, DOCmdSetAll, DOCmdSetPair

def enc_req_state_single(msg: DOReqStateSingle) -> bytes:
    return bytes([msg.ch & 0xFF])

def enc_req_state_all() -> bytes:
    return b""

def enc_cmd_set_single(msg: DOCmdSetSingle) -> bytes:
    return bytes([msg.ch & 0xFF, (1 if msg.value else 0) & 0xFF])

def enc_cmd_set_all(msg: DOCmdSetAll) -> bytes:
    return write_u32_be(msg.bitmap & 0xFFFFFFFF)

def enc_cmd_set_pair(msg: DOCmdSetPair) -> bytes:
    return bytes([msg.ch_a & 0xFF, msg.ch_b & 0xFF, msg.state2b & 0b11])
