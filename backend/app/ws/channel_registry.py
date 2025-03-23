import os
import importlib
from pathlib import Path

CHANNELS = {}

channels_dir = Path(__file__).parent / "channels"

for file in os.listdir(channels_dir):
    if file.endswith(".py") and not file.startswith("__"):
        module_name = f"app.ws.channels.{file[:-3]}"
        try:
            module = importlib.import_module(module_name)
            if hasattr(module, "get_channel_config"):
                config = module.get_channel_config()
                name = config["name"]
                CHANNELS[name] = config
        except Exception as e:
            print(f"❌ Failed to load channel module {module_name}: {e}")
