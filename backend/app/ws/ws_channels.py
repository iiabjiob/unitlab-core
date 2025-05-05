def system_info_channel() -> str:
    return "system_info"

def time_status_channel() -> str:
    return "time_status"

def device_register_channel() -> str:
    return "devices/register"

def unit_states_channel(unit_id: str) -> str:
    return f"devices/{unit_id}/states"