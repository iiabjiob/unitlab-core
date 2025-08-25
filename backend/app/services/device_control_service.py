from app.infrastructure.mqtt.gmqtt_client import UnitLabMqttClient
from app.core.logger import get_logger

logger = get_logger("mqtt")

def scan_devices_now():
    topic = "TBC"
    logger.info(f"📤 OUT → {topic} | payload=(empty)")
    
    mqtt_client = UnitLabMqttClient.get_instance()
    mqtt_client.publish(topic, b"")

def request_state_now(device_type: str, unit_id: str):
    topic = "TBC"
    logger.info(f"📤 OUT → {topic} | payload=(empty)")

    mqtt_client = UnitLabMqttClient.get_instance()
    mqtt_client.publish(topic, b"")

def set_do_command_now(
    unit_id: str,
    mode: int,
    delay_before_ms: int,
    pulse_ms: int,
    repeat: int,
    bitmask: int,
):
    topic = "TBC"
    payload = "NONE. TO BE BUILD BY PROTOCOL"
    logger.info(
        f"📤 OUT → {topic} | payload={payload.hex().upper()} "
        f"(mode={mode}, delay={delay_before_ms}, pulse={pulse_ms}, repeat={repeat}, bitmask=0x{bitmask:08X})"
    )
    
    mqtt_client = UnitLabMqttClient.get_instance()
    mqtt_client.publish(topic, payload)
