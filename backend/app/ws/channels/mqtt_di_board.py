from app.mqtt.subscription_manager import subscription_manager

def get_channel_config(channel_name: str):
    """
    Обработка каналов вида mqtt_di_unit/di-unit-1
    """
    unit_id = channel_name.split("/", 1)[1]
    topic = f"{unit_id}/status/#"

    return {
        "name": channel_name,
        "enabled": True,
        "provider": None,
        "on_subscribe": lambda ws: subscription_manager.subscribe(topic, ws),
        "on_unsubscribe": lambda ws: subscription_manager.unsubscribe(topic, ws),
    }
