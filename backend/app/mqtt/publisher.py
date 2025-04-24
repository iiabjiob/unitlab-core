import json
from paho.mqtt.client import Client
from app.mqtt import topics
from app.core.logger import get_logger

logger = get_logger("mqtt")

mqtt_client: Client = None  # будет установлен извне

def init_mqtt_client(client: Client):
    global mqtt_client
    mqtt_client = client

def publish(topic: str, payload: str = "{}", qos: int = 0, retain: bool = False):
    try:
        if mqtt_client is None:
            raise RuntimeError("Client is not initialized")

        mqtt_client.publish(topic, payload=payload, qos=qos, retain=retain)
        logger.debug(f"→ {topic} ← {payload}")
    except Exception as e:
        logger.error(f"❌ Publish error to {topic}: {e}")

def publish_json(topic: str, data: dict, qos: int = 0, retain: bool = False):
    try:
        payload = json.dumps(data)
        publish(topic, payload, qos, retain)
    except Exception as e:
        logger.error(f"❌ Publish_json error to {topic}: {e}")

def publish_status(unit_id: str, pin: int, state: bool):
    topic = topics.status(unit_id, pin)
    payload = "true" if state else "false"
    publish(topic, payload)
