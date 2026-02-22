# app/infrastructure/mqtt/gmqtt_client.py
import asyncio
from gmqtt import Client as MQTTClient
from typing import Callable, Awaitable, Optional
from app.core.logger import get_logger

logger = get_logger("mqtt")

OnMessageAsync = Callable[[str, bytes, int, object], Awaitable[None]]

class UnitLabMqttClient:
    """Low-level async MQTT client (gmqtt wrapper). Keeps zero app logic inside."""
    def __init__(self, client_id: str):
        self.client = MQTTClient(client_id)
        self.connected = asyncio.Event()
        self._subscriptions: dict[str, int] = {}

        # External async message hook (set by manager)
        self._on_message_async: Optional[OnMessageAsync] = None

        # Bind gmqtt callbacks
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message
        self.client.on_subscribe = self.on_subscribe

    def set_on_message(self, handler: OnMessageAsync):
        """Register async on_message hook owned by higher layer (manager)."""
        self._on_message_async = handler

    async def connect(self, host: str, port: int):
        await self.client.connect(host, port)
        logger.info("✅ MQTT client started")

    async def disconnect(self):
        await self.client.disconnect()
        logger.info("🛑 MQTT client stopped")
    
    def subscribe(self, topic: str, qos: int = 0):
        self._subscriptions[topic] = qos
        if self.connected.is_set():
            self.client.subscribe(topic, qos)
            logger.info(f"📡 Subscribing to topic: {topic}")

    def publish(self, topic, payload, qos=0, retain=False):
        return self.client.publish(topic, payload, qos=qos, retain=retain)

    # --- gmqtt callbacks ---
    def on_connect(self, client, flags, rc, properties):
        logger.info("✅ Connected to MQTT broker")
        self.connected.set()
        for topic, qos in self._subscriptions.items():
            self.client.subscribe(topic, qos)
            logger.info(f"📡 Subscribing to topic: {topic}")

    def on_disconnect(self, client, packet, exc=None):
        logger.warning("⚠️ Disconnected from MQTT broker")
        self.connected.clear()

    def on_subscribe(self, client, mid, qos, properties):
        logger.info(f"✅ Subscribed (mid={mid}, qos={qos})")

    def on_message(self, client, topic, payload, qos, properties):
        # Delegate to manager-provided async hook (don’t block gmqtt callback)
        if self._on_message_async:
            asyncio.create_task(self._on_message_async(topic, payload, qos, properties))
        else:
            logger.debug(f"📥 Received (no handler set): {topic} ({len(payload)} bytes)")
