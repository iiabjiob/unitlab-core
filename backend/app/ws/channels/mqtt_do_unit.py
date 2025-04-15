from app.mqtt.subscription_manager import subscription_manager
from app.mqtt.client import mqtt_client

def get_channel_config(channel_name: str):
    """
    Обработка каналов вида mqtt_do_unit/do-unit-XXXX
    """
    unit_id = channel_name.split("/", 1)[1]

    status_sub_topic = f"{unit_id}/status/#"
    request_status_topic = f"{unit_id}/get/status"

    return {
        "name": channel_name,
        "enabled": True,
        "provider": None,
        "on_subscribe": lambda ws: handle_subscribe(ws, status_sub_topic, request_status_topic),
        "on_unsubscribe": lambda ws: subscription_manager.unsubscribe(status_sub_topic, ws),
    }

def handle_subscribe(ws, status_topic: str, request_topic: str):
    # 📡 Подписка на получение MQTT-запроса на получение текущих статусов
    subscription_manager.subscribe(status_topic, ws)

    # 📡 Отправка MQTT-запроса на получение текущих статусов
    mqtt_client.publish(request_topic, payload="{}", qos=0, retain=False)