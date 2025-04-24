ALLOWED_PUBLISH_PREFIXES = (
    "do-unit-",
    "di-unit-",
)
def is_allowed_publish_topic(topic: str) -> bool:
    return topic.startswith(ALLOWED_PUBLISH_PREFIXES)

# device registration
REGISTER_ROOT = "device/register"
def register_announce(unit_id: str) -> str:
    return f"{REGISTER_ROOT}/{unit_id}"

# device control
def set_pin(unit_id: str, index: int) -> str:
    return f"{unit_id}/set/{index}"

def set_group(unit_id: str) -> str:
    return f"{unit_id}/set/group"

# status publishing
def status(unit_id: str, index: int) -> str:
    return f"{unit_id}/status/{index}"

# status request
def get_status(unit_id: str) -> str:
    return f"{unit_id}/get/status"
