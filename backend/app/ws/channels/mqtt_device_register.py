from app.mqtt.subscription_manager import subscription_manager
from app.mqtt.publisher import publish  # или publish_json, если нужно
from app.mqtt.topics import device_register, device_scan

def get_channel_config():
    return {
        "name": "mqtt_device_register",
        "enabled": False,  # Не стартует как таск
        "provider": None,  # Push-only
        "on_subscribe": lambda ws: (
            subscription_manager.subscribe(device_register(), ws) or
            publish(device_scan())
        ),
        "on_unsubscribe": lambda ws: subscription_manager.unsubscribe(device_register(), ws),
    }
