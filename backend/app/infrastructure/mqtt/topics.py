DEVICE_STATE     = "+/s"
DEVICE_REQ_STATE = "+/q"
DEVICE_HEARTBEAT = "+/h"
DEVICE_HEARTBEAT_DIAG = "+/hd"
DEVICE_CMD       = "+/c"
DEVICE_RESP      = "+/r"
DEVICE_INFO      = "+/i"
DEVICE_REGISTER  = "+/reg"
DEVICE_SCAN      = "scan"

CORE_TOPICS = [
    DEVICE_STATE,
    DEVICE_HEARTBEAT,
    DEVICE_HEARTBEAT_DIAG,
    DEVICE_RESP,
    DEVICE_REGISTER,
]

# Publish-only topics (Core → devices)
PUBLISH_ONLY_TOPICS = [
    DEVICE_CMD,
    DEVICE_REQ_STATE,
    DEVICE_SCAN,
    DEVICE_INFO,
]

# Topic helpers
def register(unit_id: str) -> str:
    return f"{unit_id}/reg"

def heartbeat(unit_id: str) -> str:
    return f"{unit_id}/h"

def heartbeat_diag(unit_id: str) -> str:
    return f"{unit_id}/hd"

def state(unit_id: str) -> str:
    return f"{unit_id}/s"

def state_channel(unit_id: str, ch: int) -> str:
    return f"{unit_id}/s/{ch}"

def cmd(unit_id: str) -> str:
    return f"{unit_id}/c"

def req_state(unit_id: str) -> str:
    return f"{unit_id}/q"

def resp(unit_id: str) -> str:
    return f"{unit_id}/r"

def info(unit_id: str) -> str:
    return f"{unit_id}/i"
