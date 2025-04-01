from app.mqtt.subscription_manager import subscription_manager

def get_channel_config():
    return {
        "name": "mqtt_do_boards",
        "enabled": True,
        "provider": None,
        "on_subscribe": lambda ws: subscription_manager.subscribe("do-board-1/status/#", ws),
        "on_unsubscribe": lambda ws: subscription_manager.unsubscribe("do-board-1/status/#", ws),
    }