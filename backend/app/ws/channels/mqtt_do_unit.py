from app.mqtt.subscription_manager import subscription_manager

def get_channel_config(channel_name: str):
    """
    Обработка каналов вида mqtt_do_unit/do-unit-XXXX
    """
    unit_id = channel_name.split("/", 1)[1]

    status_sub_topic = f"{unit_id}/status/#"
    
    return {
        "name": channel_name,
        "enabled": True,
        "provider": None,
        "on_subscribe": lambda ws: subscription_manager.subscribe(status_sub_topic, ws),
        "on_unsubscribe": lambda ws: subscription_manager.unsubscribe(status_sub_topic, ws),
    }