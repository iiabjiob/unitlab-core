import json
from app.mqtt.client import client
from app.mqtt.topics import set_pin, get_states, device_scan
from app.core.logger import get_logger

logger = get_logger("mqtt")

def scan_devices_now():
    topic = device_scan()
    payload = "{}"
    logger.info(f"📤 OUT → {topic} | payload={payload}")
    client.publish(topic, payload)

def request_states_now(unit_id: str):
    topic = get_states(unit_id)
    logger.info(f"📤 OUT → {topic} | payload={{}}")
    client.publish(topic, "{}")

def set_pin_now(unit_id: str, index: int, value: bool):
    topic = set_pin(unit_id, index)
    payload = {"state": value}
    logger.info(f"📤 OUT → {topic} | payload={payload}")
    client.publish(topic, json.dumps(payload))