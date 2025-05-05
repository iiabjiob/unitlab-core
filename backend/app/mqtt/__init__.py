# app/mqtt/__init__.py
import paho.mqtt.client as mqtt
from .mqtt_router import MQTTRouter
from app.core.logger import get_logger

from .client import client  # ваш paho.Client()

# 1) захватываем asyncio-loop прямо на старте
import asyncio
_loop = asyncio.get_running_loop()

# 2) создаём роутер с этим loop
router = MQTTRouter(client, loop=_loop)

# 3) импортируем все хендлеры — декораторы тут же подпишут их
import app.mqtt.handlers.device_register
import app.mqtt.handlers.unit_states
import app.mqtt.handlers.unit_single_state
# import app.mqtt.handlers.other...
