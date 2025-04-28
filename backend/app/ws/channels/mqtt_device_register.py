from app.mqtt.subscription_manager import subscription_manager
from app.mqtt.topics import device_register
from app.mqtt.handlers.device_register_handler import handle_device_register_message

def get_channel_config():
    return {
        "name": "mqtt_device_register",
        "enabled": False,  # Не стартует как таск
        "provider": None,  # Push-only
        "on_subscribe": lambda ws: (
            subscription_manager.subscribe(device_register(), ws),
            subscription_manager.subscribe(device_register(), handle_device_register_message)
        ),
        "on_unsubscribe": lambda ws: subscription_manager.unsubscribe(device_register(), ws),
    }
