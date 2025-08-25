
from ..common.messages import SysResp

def dec_resp(payload: bytes) -> SysResp:
    if len(payload) != 2:
        raise ValueError(f"SYS.RESP payload must be 2 bytes, got {len(payload)}")
    return SysResp(status=payload[0], err_code=payload[1])
