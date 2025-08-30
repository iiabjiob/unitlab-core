# Шаблоны подписок
DEVICE_STATE     = "unitlab/devices/+/+/state"
DEVICE_REQ_STATE = "unitlab/devices/+/+/req/state"
DEVICE_HEARTBEAT = "unitlab/devices/+/+/heartbeat"
DEVICE_CMD       = "unitlab/devices/+/+/cmd"
DEVICE_RESP      = "unitlab/devices/+/+/resp"
DEVICE_REGISTER  = "unitlab/device/register/#"
DEVICE_SCAN      = "unitlab/devices/scan"

# Шаблоны подписок для Core (только входящие от периферии)
CORE_TOPICS = [
    DEVICE_STATE,
    DEVICE_HEARTBEAT,
    DEVICE_RESP,
    DEVICE_REGISTER,
]

# Эти только для публикации Core → периферия
PUBLISH_ONLY_TOPICS = [
    DEVICE_CMD,
    DEVICE_REQ_STATE,
    DEVICE_SCAN,
]

# Генераторы конкретных топиков
def base(type: str, unit_id: str, namespace: str = "unitlab") -> str:
    return f"{namespace}/devices/{type}/{unit_id}"

def heartbeat(type: str, unit_id: str, namespace: str = "unitlab") -> str:
    return f"{base(type, unit_id, namespace)}/heartbeat"

def state(type: str, unit_id: str, namespace: str = "unitlab") -> str:
    return f"{base(type, unit_id, namespace)}/state"

def state_channel(type: str, unit_id: str, ch: int, namespace: str = "unitlab") -> str:
    return f"{base(type, unit_id, namespace)}/state/ch/{ch}"

def cmd(type: str, unit_id: str, namespace: str = "unitlab") -> str:
    return f"{base(type, unit_id, namespace)}/cmd"

def req_state(type: str, unit_id: str, namespace: str = "unitlab") -> str:
    return f"{base(type, unit_id, namespace)}/req/state"

def resp(type: str, unit_id: str, namespace: str = "unitlab") -> str:
    return f"{base(type, unit_id, namespace)}/resp"

def register(unit_id: str, namespace: str = "unitlab") -> str:
    return f"{namespace}/device/register/{unit_id}"

def scan(namespace: str = "unitlab") -> str:
    return f"{namespace}/devices/scan"
