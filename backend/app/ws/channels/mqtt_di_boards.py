from app.mqtt.subscription_manager import subscription_manager

def get_channel_config():
    return {
        "name": "mqtt_di_boards",
        "enabled": True,
        "provider": None,
        "on_subscribe": lambda ws: subscription_manager.subscribe("di-board-1/status/#", ws),
        "on_unsubscribe": lambda ws: subscription_manager.subscribe("di-board-1/status/#", ws),
    }
