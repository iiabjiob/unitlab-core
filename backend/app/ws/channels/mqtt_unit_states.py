from app.mqtt.subscription_manager import subscription_manager
from app.mqtt.publisher import publish
from app.core.logger import get_logger

logger = get_logger("mqtt")

def get_channel_config(channel_name: str):
    """
    Обработка каналов вида mqtt_dX_unit/dX-unit-XXXX
    """
    unit_id = channel_name.split("/", 1)[1]

    state_topic = f"{unit_id}/state/#"
    states_topic = f"{unit_id}/states"
    get_states_topic = f"{unit_id}/get/states"
    
    return {
        "name": channel_name,
        "enabled": True,
        "provider": None,
        "on_subscribe": lambda ws: (
            logger.info(f"🔔 Subscribing WebSocket to {state_topic} and {states_topic}"),
            subscription_manager.subscribe(state_topic, ws),
            subscription_manager.subscribe(states_topic, ws),
            logger.info(f"📤 Requesting state from unit: {get_states_topic}"),
            publish(get_states_topic)
        ),
        "on_unsubscribe": lambda ws: (
            logger.info(f"🔕 Unsubscribing WebSocket from {state_topic} and {states_topic}"),
            subscription_manager.unsubscribe(state_topic, ws),
            subscription_manager.unsubscribe(states_topic, ws)
        ),
    }