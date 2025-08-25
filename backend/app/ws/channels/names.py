# Статичные каналы (глобальные)
SYSTEM_INFO     = "system_info"
TIME_STATUS     = "time_status"
DEVICES_STATUS  = "devices/status"         # online/offline событий всех устройств
DEVICE_REGISTER = "device/register"        # регистрация устройств (как в MQTT)

# Динамические каналы (по устройствам)
def device_state(device_type: str, unit_id: str) -> str:
    return f"devices/{device_type}/{unit_id}/state"
