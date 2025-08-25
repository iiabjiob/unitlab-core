"""
Import all handler modules to ensure they self-register in the MQTT router.
Call this from startup (main.py).
"""
from . import device_state
from . import device_register
from . import device_heartbeat