# Статичные каналы (глобальные)
SYSTEM_INFO     = "system/info"
TIME_STATUS     = "time/status"

DEVICES_RESP     = "devices/resp"
DEVICES_STATUS   = "devices/status"         # online/offline событий всех устройств
DEVICES_REGISTER = "devices/register"        # регистрация устройств (как в MQTT)

# Динамические каналы (по устройствам)
def device_state(device_type: str, unit_id: str) -> str:
    return f"devices/{device_type}/{unit_id}/state"
