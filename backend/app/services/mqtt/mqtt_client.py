import json
import paho.mqtt.client as mqtt
from app.core.logger import logger
from app.core.config import get_settings

settings = get_settings()

class MQTTClient:
    def __init__(self, topic: str, broker: str = None, port: int = None):
        """Initialize MQTT client with parameters"""
        self.broker = broker or settings.host
        self.port = port or settings.mqtt_port
        self.topic = topic  # Topic specified when creating an object

        # Create the MQTT client
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect

        try:
            self.client.connect(self.broker, self.port, 60)
            self.client.loop_start()  # Start MQTT loop
            logger.info(f"✅ MQTT connected to {self.broker}:{self.port}, topic: {self.topic}")
        except Exception as e:
            logger.error(f"❌ MQTT connection error: {e}")

    def on_connect(self, client, userdata, flags, rc):
        """Handle successful connection"""
        if rc == 0:
            logger.info(f"🔗 Successfully connected to MQTT ({self.topic}).")
        else:
            logger.error(f"⚠️ MQTT connection failed: code {rc}")

    def on_disconnect(self, client, userdata, rc):
        """Handle disconnection from the broker"""
        logger.warning(f"🔌 Disconnected from MQTT ({self.topic}), code {rc}")

    def publish(self, message):
        """Publish a message to MQTT"""
        try:
            self.client.publish(self.topic, json.dumps(message))
            logger.debug(f"📡 Sent to MQTT ({self.topic}): {message}")
        except Exception as e:
            logger.error(f"❌ MQTT publish error ({self.topic}): {e}")

    def stop(self):
        """Stop the MQTT client"""
        self.client.loop_stop()
        self.client.disconnect()
        logger.info(f"🔴 MQTT client ({self.topic}) stopped.")
