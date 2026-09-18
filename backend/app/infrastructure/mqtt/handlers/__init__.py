"""Import MQTT handlers so their topic patterns register at startup."""

# These imports are intentional side effects: every handler registers its
# topic pattern with the shared registry when the module is imported.
from app.infrastructure.mqtt.handlers import device_heartbeat as _device_heartbeat  # noqa: F401
from app.infrastructure.mqtt.handlers import device_register as _device_register  # noqa: F401
from app.infrastructure.mqtt.handlers import device_resp as _device_resp  # noqa: F401
from app.infrastructure.mqtt.handlers import device_state as _device_state  # noqa: F401
