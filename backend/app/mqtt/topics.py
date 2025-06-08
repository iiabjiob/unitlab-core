# app/mqtt/topics.py

DEVICE_STATE = "unitlab/devices/+/+/state"
DEVICE_HEARTBEAT = "unitlab/devices/+/+/heartbeat"
DEVICE_REGISTER = "unitlab/device/register/#"

DEFAULT_TOPICS = [
    DEVICE_STATE,
    DEVICE_HEARTBEAT,
    DEVICE_REGISTER,
    # Добавь любые новые топики здесь!
]

# Генераторы конкретных топиков
def set_do_command(unit_id: str) -> str:
    return f"unitlab/devices/do/{unit_id}/set"

def request_state(device_type: str, unit_id: str) -> str:
    return f"unitlab/devices/{device_type}/{unit_id}/requeststate"

def device_scan() -> str:
    return "unitlab/devices/scan"