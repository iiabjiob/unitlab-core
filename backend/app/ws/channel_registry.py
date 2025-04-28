# channel_registry.py
import os
import importlib
from pathlib import Path

CHANNELS = {}  # static
DYNAMIC_CHANNELS = []  # (prefix, getter)

channels_dir = Path(__file__).parent / "channels"

for file in os.listdir(channels_dir):
    if file.endswith(".py") and not file.startswith("__"):
        module_name = f"app.ws.channels.{file[:-3]}"
        prefix = file[:-3]  # имя файла без .py

        try:
            module = importlib.import_module(module_name)
            if hasattr(module, "get_channel_config"):
                func = module.get_channel_config

                if func.__code__.co_argcount == 0:
                    config = func()
                    if config and "name" in config:
                        CHANNELS[config["name"]] = config
                else:
                    # dynamic: канал начинается с имени файла
                    DYNAMIC_CHANNELS.append((prefix, func))
            
        except Exception as e:
            print(f"❌ Failed to load channel module {module_name}: {e}")

def get_channel(channel_name: str):
    # статический
    if channel_name in CHANNELS:
        return CHANNELS[channel_name]

    # динамический
    for prefix, func in DYNAMIC_CHANNELS:
        if channel_name.startswith(prefix + "/"):
            return func(channel_name)

    return None
