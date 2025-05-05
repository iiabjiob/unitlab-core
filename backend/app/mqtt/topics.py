ALLOWED_PUBLISH_PREFIXES = (
    "do-unit-",
    "di-unit-",
    "devices/scan",
)

def is_allowed_publish_topic(topic: str) -> bool:
    return topic.startswith(ALLOWED_PUBLISH_PREFIXES)

# --- 🚀 Устройство: сканирование и регистрация ---
def device_scan() -> str:
    return "devices/scan"

def device_register_topic() -> str:
    return "device/register/#"  # для подписки на регистрацию

def device_register() -> str:
    return "device/register/"  # для подписки на регистрацию

def states_all() -> str:
    return "+/states"

def state_all() -> str:
    return "+/state/#"

# --- 🎛 Управление состоянием (DO / Group) ---
def set_pin(unit_id: str, index: int) -> str:
    return f"{unit_id}/set/{index}"

def set_group(unit_id: str) -> str:
    return f"{unit_id}/set/group"


# --- 📤 Публикация состояний (например, от устройства) ---
def state(unit_id: str, index: int) -> str:
    return f"{unit_id}/state/{index}"

def states(unit_id: str) -> str:
    return f"{unit_id}/states"


# --- 📥 Запрос текущих состояний ---
def get_states(unit_id: str) -> str:
    return f"{unit_id}/get/states"