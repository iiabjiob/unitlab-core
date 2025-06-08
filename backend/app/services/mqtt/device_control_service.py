from app.mqtt.gmqtt_client import UnitLabMqttClient
from app.mqtt.topics import set_do_command, request_state, device_scan
from app.core.logger import get_logger
from app.core.protocol import build_do_command_packet

logger = get_logger("mqtt")

def scan_devices_now():
    topic = device_scan()
    logger.info(f"📤 OUT → {topic} | payload=(empty)")
    
    mqtt_client = UnitLabMqttClient.get_instance()
    mqtt_client.publish(topic, b"")

def request_state_now(device_type: str, unit_id: str):
    topic = request_state(device_type, unit_id,)
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
    topic = set_do_command(unit_id)  # например: unitlab/devices/do/<unit_id>/set
    payload = build_do_command_packet(mode, delay_before_ms, pulse_ms, repeat, bitmask)
    logger.info(
        f"📤 OUT → {topic} | payload={payload.hex().upper()} "
        f"(mode={mode}, delay={delay_before_ms}, pulse={pulse_ms}, repeat={repeat}, bitmask=0x{bitmask:08X})"
    )
    
    mqtt_client = UnitLabMqttClient.get_instance()
    mqtt_client.publish(topic, payload)
