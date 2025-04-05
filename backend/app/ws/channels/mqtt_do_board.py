from app.mqtt.subscription_manager import subscription_manager

def get_channel_config(channel_name: str):
    """
    Обработка каналов вида mqtt_do_board/do-board-1
    """
    board_id = channel_name.split("/", 1)[1]
    topic = f"{board_id}/status/#"

    return {
        "name": channel_name,
        "enabled": True,
        "provider": None,
        "on_subscribe": lambda ws: subscription_manager.subscribe(topic, ws),
        "on_unsubscribe": lambda ws: subscription_manager.unsubscribe(topic, ws),
    }
