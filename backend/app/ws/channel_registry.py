from app.ws.channels import time_status
from app.ws.channels import system_info

# Собираем конфиги только один раз
time_status_config = time_status.get_channel_config()
system_info_config = system_info.get_channel_config()

CHANNELS = {
    time_status_config["name"]: time_status_config,
    system_info_config["name"]: system_info_config,
    # сюда можно руками добавить другие каналы
}

DYNAMIC_CHANNELS = []  # пока не используем

def get_channel(channel_name: str):
    # статический
    if channel_name in CHANNELS:
        return CHANNELS[channel_name]

    # динамический (если появятся)
    for prefix, func in DYNAMIC_CHANNELS:
        if channel_name.startswith(prefix + "/"):
            return func(channel_name)

    return None
