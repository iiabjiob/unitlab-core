from fastapi import FastAPI
from importlib import import_module
from pathlib import Path

def register_routers(app: FastAPI):
    current_dir = Path(__file__).parent
    for file in current_dir.glob("*.py"):
        if file.stem == "api_manager" or file.stem.startswith("_"):
            continue

        module_name = f"{__package__}.{file.stem}"  # example: app.api.status
        module = import_module(module_name)

        if hasattr(module, "router"):
            app.include_router(module.router)
